
#!/usr/bin/env bash

set -Eeuo pipefail

###############################################################################
# Cobalt 27 / Larboard RDK ARM build using a prebuilt Yocto SDK
#
# All SDK files, source repositories and build outputs are kept inside
# this Larboard repository workspace.
#
# Local testing:
#   bash cov_build.sh
#
# Future Artifactory testing:
#   SDK_INSTALLER_URL="<Artifactory installer URL>" bash cov_build.sh
###############################################################################

# Resolve repository directory from this script's location.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"

git config --global --add safe.directory "$ROOT_DIR"
cd "$ROOT_DIR"

###############################################################################
# Configuration
###############################################################################

# Prebuilt Yocto SDK committed to this repository using Git LFS.
SDK_DIR="$ROOT_DIR/oecore-x86_64"

# Create original SDK path for GitHub Actions.
# The Yocto compiler has this path embedded as its ELF interpreter.
if [ "${GITHUB_ACTIONS:-}" = "true" ]; then
    ORIGINAL_SDK_PATH="/home/sranan984/oecore-x86_64"

    if [ -e "$ORIGINAL_SDK_PATH" ] || [ -L "$ORIGINAL_SDK_PATH" ]; then
        if [ "$(readlink -f "$ORIGINAL_SDK_PATH")" != "$(readlink -f "$SDK_DIR")" ]; then
            echo "ERROR: SDK path already exists and points elsewhere: $ORIGINAL_SDK_PATH"
            exit 1
        fi
    else
        mkdir -p "$(dirname "$ORIGINAL_SDK_PATH")"
        ln -s "$SDK_DIR" "$ORIGINAL_SDK_PATH"
        echo "Created SDK symlink: $ORIGINAL_SDK_PATH -> $SDK_DIR"
    fi

    ls -l "$ORIGINAL_SDK_PATH"
fi

# Create the original SDK path expected by the Yocto compiler.
ORIGINAL_SDK_PATH="/home/sranan984/oecore-x86_64"

if [ "$SDK_DIR" != "$ORIGINAL_SDK_PATH" ]; then
    mkdir -p "$(dirname "$ORIGINAL_SDK_PATH")"

    if [ -e "$ORIGINAL_SDK_PATH" ] || [ -L "$ORIGINAL_SDK_PATH" ]; then
        if [ "$(readlink -f "$ORIGINAL_SDK_PATH")" != "$(readlink -f "$SDK_DIR")" ]; then
            echo "ERROR: Original SDK path already exists and points elsewhere: $ORIGINAL_SDK_PATH"
            exit 1
        fi
    else
        ln -s "$SDK_DIR" "$ORIGINAL_SDK_PATH"
        echo "Created SDK symlink: $ORIGINAL_SDK_PATH -> $SDK_DIR"
    fi
fi

SDK_SETUP="$SDK_DIR/environment-setup-armv7at2hf-neon-oe-linux-gnueabi"
SDK_SETUP_RUNTIME=""

DEPOT_TOOLS_DIR="$ROOT_DIR/depot_tools"
COBALT_ROOT="$ROOT_DIR/cobalt-build"
COBALT_DIR="$COBALT_ROOT/src"
LARBOARD_DIR="$COBALT_ROOT/larboard"

COBALT_REPO="https://github.com/youtube/cobalt.git"
COBALT_TAG="27.lts.3"

LARBOARD_REPO="https://github.com/rdkcentral/larboard.git"
LARBOARD_COMMIT="980732e5ad863aefd2692307e08e36e3e325e07a"

DEPOT_TOOLS_REPO="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_COMMIT="2cea9cdd785391335d448773124631cb711225e2"

GN_OUT="$COBALT_DIR/out/rdk-arm_gold"
NINJA_JOBS="${NINJA_JOBS:-4}"

###############################################################################
# Helper functions
###############################################################################

log() {
    echo
    echo "================================================================"
    echo " $1"
    echo "================================================================"
}

fail() {
    echo
    echo "ERROR: $1"
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 ||
        fail "Required command not found: $1"
}

clone_or_update() {
    local repo_url="$1"
    local repo_dir="$2"

    mkdir -p "$(dirname "$repo_dir")"

    if [[ -d "$repo_dir/.git" ]]; then
        echo "Reusing existing repository: $repo_dir"
        git -C "$repo_dir" remote set-url origin "$repo_url"
        git -C "$repo_dir" fetch --tags --prune origin
    else
        if [[ -e "$repo_dir" ]]; then
            fail "$repo_dir exists but is not a Git repository. Please inspect it."
        fi

        echo "Cloning $repo_url"
        git clone "$repo_url" "$repo_dir"
    fi
}

checkout_commit() {
    local repo_dir="$1"
    local commit="$2"

    git -C "$repo_dir" fetch origin "$commit" ||
        git -C "$repo_dir" fetch --tags origin

    git -C "$repo_dir" cat-file -e "${commit}^{commit}" ||
        fail "Commit $commit is not available in $repo_dir"

    git -C "$repo_dir" checkout --detach "$commit"
}

###############################################################################
# Phase 1: Check host tools
###############################################################################

log "Phase 1: Checking host tools"

for tool in git curl python3 make gcc g++ tar; do
    require_command "$tool"
done

# SDK is already present in the repository.

###############################################################################
# Phase 2: Verify the prebuilt Yocto SDK
###############################################################################

log "Phase 2: Verifying prebuilt Yocto SDK"

[[ -d "$SDK_DIR" ]] ||
    fail "SDK directory not found: $SDK_DIR"

[[ -f "$SDK_SETUP" ]] ||
    fail "SDK environment setup file not found: $SDK_SETUP"

[[ -d "$SDK_DIR/sysroots/armv7at2hf-neon-oe-linux-gnueabi" ]] ||
    fail "SDK target sysroot is missing."

[[ -d "$SDK_DIR/sysroots/x86_64-bolt_distro-linux" ]] ||
    fail "SDK host sysroot is missing."

echo "Using repository SDK: $SDK_DIR"
du -sh "$SDK_DIR"

###############################################################################
# Phase 3: Initialize the Yocto SDK
###############################################################################

log "Phase 3: Initializing Yocto SDK"

# The original SDK setup file contains the DVM installation path.
# Rewrite that path in a temporary copy so the SDK works from any workspace.
SDK_SETUP_RUNTIME="$(mktemp)"

sed "s|/home/sranan984/oecore-x86_64|$SDK_DIR|g" \
    "$SDK_SETUP" > "$SDK_SETUP_RUNTIME"

if grep -q '/home/sranan984/oecore-x86_64' "$SDK_SETUP_RUNTIME"; then
    fail "SDK runtime setup still contains the original DVM path."
fi

# Remove the temporary setup file when the script exits.
trap 'rm -f "$SDK_SETUP_RUNTIME"' EXIT

echo "Initializing SDK environment from: $SDK_DIR"

# Yocto SDK setup scripts may reference unset shell variables.
set +u
source "$SDK_SETUP_RUNTIME"

# Use host system CA certificates instead of SDK build-time certificate path.
unset SSL_CERT_FILE SSL_CERT_DIR
export GIT_SSL_CAINFO=/etc/ssl/certs/ca-certificates.crt
export CURL_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt

test -f "$GIT_SSL_CAINFO" || {
    echo "ERROR: Host CA certificate bundle not found"
    exit 1
}

git config --global http.sslCAInfo "$GIT_SSL_CAINFO"
echo "Git SSL certificate: $GIT_SSL_CAINFO"

set -u

# Required for the successful manual RDK ARM build.
unset PKG_CONFIG_SYSROOT_DIR || true

# Verify the ARM cross-compiler and target sysroot.
require_command arm-oe-linux-gnueabi-gcc
require_command arm-oe-linux-gnueabi-g++

[[ -d "$SDKTARGETSYSROOT" ]] ||
    fail "Target sysroot not found: $SDKTARGETSYSROOT"

echo "ARM compiler:"
arm-oe-linux-gnueabi-gcc --version | head -1

echo "Compiler target:"
arm-oe-linux-gnueabi-gcc -dumpmachine

echo "Target sysroot: $SDKTARGETSYSROOT"

export DEPOT_TOOLS_UPDATE=0
export DEPOT_TOOLS_METRICS=0

echo "CC: $CC"
echo "CXX: $CXX"
echo "Target sysroot: $SDKTARGETSYSROOT"

###############################################################################
# Phase 4: Prepare depot_tools
###############################################################################

log "Phase 4: Preparing depot_tools"

clone_or_update "$DEPOT_TOOLS_REPO" "$DEPOT_TOOLS_DIR"
checkout_commit "$DEPOT_TOOLS_DIR" "$DEPOT_TOOLS_COMMIT"

export PATH="$DEPOT_TOOLS_DIR:$PATH"

if [[ -f "$DEPOT_TOOLS_DIR/bootstrap_python3" ]]; then
    cd "$DEPOT_TOOLS_DIR"
    source ./bootstrap_python3

    if declare -F bootstrap_python3 >/dev/null 2>&1; then
        bootstrap_python3
    fi
fi

export PATH="$DEPOT_TOOLS_DIR:$PATH"

require_command gclient

###############################################################################
# Phase 5: Prepare Cobalt workspace and synchronize dependencies
###############################################################################

log "Phase 5: Preparing Cobalt workspace"

mkdir -p "$COBALT_ROOT"
cd "$COBALT_ROOT"

if [[ ! -f .gclient ]]; then
    cat > .gclient <<'GCLIENT'
solutions = [
  {
    "name": "src",
    "url": "https://github.com/youtube/cobalt.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]

checkout_configuration = "small"
GCLIENT
fi

echo "Synchronizing Cobalt dependencies..."

gclient sync --nohooks

if [[ ! -d "$COBALT_DIR/.git" ]]; then
    fail "Cobalt source directory was not created by gclient."
fi

git -C "$COBALT_DIR" fetch origin --tags
git -C "$COBALT_DIR" checkout --detach "$COBALT_TAG"

echo "Checking out Cobalt version: $COBALT_TAG"

###############################################################################
# Phase 6: Prepare Larboard
###############################################################################

log "Phase 6: Preparing Larboard source"

clone_or_update "$LARBOARD_REPO" "$LARBOARD_DIR"
checkout_commit "$LARBOARD_DIR" "$LARBOARD_COMMIT"

echo "Larboard commit:"
git -C "$LARBOARD_DIR" log -1 --oneline

###############################################################################
# Phase 7: Run Cobalt hooks
###############################################################################

log "Phase 7: Running gclient hooks"

cd "$COBALT_ROOT"
gclient runhooks

###############################################################################
# Phase 8: Configure Starboard platform
###############################################################################

log "Phase 8: Configuring RDK ARM Starboard platform"

STARBOARD_LINK="$COBALT_DIR/third_party/starboard"
STARBOARD_TARGET="$LARBOARD_DIR/src/third_party/starboard"

[[ -d "$STARBOARD_TARGET" ]] ||
    fail "Larboard Starboard source not found: $STARBOARD_TARGET"

if [[ -L "$STARBOARD_LINK" ]]; then
    CURRENT_TARGET="$(readlink -f "$STARBOARD_LINK")"
    EXPECTED_TARGET="$(readlink -f "$STARBOARD_TARGET")"

    if [[ "$CURRENT_TARGET" != "$EXPECTED_TARGET" ]]; then
        rm "$STARBOARD_LINK"
        ln -s "$STARBOARD_TARGET" "$STARBOARD_LINK"
    fi
elif [[ -e "$STARBOARD_LINK" ]]; then
    fail "$STARBOARD_LINK exists and is not a symlink. Inspect it before continuing."
else
    ln -s "$STARBOARD_TARGET" "$STARBOARD_LINK"
fi

echo "Starboard link:"
ls -ld "$STARBOARD_LINK"

###############################################################################
# Phase 9: Add RDK ARM platform to GN configuration
###############################################################################

log "Phase 9: Updating Cobalt GN platform list"

GN_PY="$COBALT_DIR/cobalt/build/gn.py"
ARGS_DIR="$COBALT_DIR/cobalt/build/configs/rdk-arm"
ARGS_FILE="$ARGS_DIR/args.gn"

[[ -f "$GN_PY" ]] || fail "Cobalt GN script not found: $GN_PY"

python3 - "$GN_PY" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
content = path.read_text()

marker = "_COBALT_STARBOARD_PLATFORMS"

start = content.find(marker)
if start == -1:
    raise SystemExit("ERROR: Could not find _COBALT_STARBOARD_PLATFORMS in gn.py")

open_bracket = content.find("[", start)
close_bracket = content.find("]", open_bracket)

if open_bracket == -1 or close_bracket == -1:
    raise SystemExit("ERROR: Could not locate the platform list in gn.py")

platform_list = content[open_bracket:close_bracket]

if "'rdk-arm'" not in platform_list and '"rdk-arm"' not in platform_list:
    content = (
        content[:close_bracket]
        + "\n    'rdk-arm',\n"
        + content[close_bracket:]
    )
    path.write_text(content)
    print("Added rdk-arm to _COBALT_STARBOARD_PLATFORMS")
else:
    print("rdk-arm is already present in GN platform list")
PY

mkdir -p "$ARGS_DIR"

cat > "$ARGS_FILE" <<'ARGS'
import("//third_party/starboard/rdk/arm/args.gn")
ARGS

echo "RDK ARM GN arguments:"
cat "$ARGS_FILE"

###############################################################################
# Phase 10: Generate GN build files
###############################################################################

log "Phase 10: Generating RDK ARM GN build files"

cd "$COBALT_DIR"

python3 "$GN_PY" --no-rbe -p rdk-arm -c gold

[[ -d "$GN_OUT" ]] ||
    fail "GN output directory was not generated: $GN_OUT"

###############################################################################
# Phase 11: Build libstarboard_platform.a
###############################################################################

log "Phase 11: Building libstarboard_platform.a"

require_command ninja

ninja -C "$GN_OUT" -j"$NINJA_JOBS" \
    starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a

###############################################################################
# Phase 12: Verify build output
###############################################################################

log "Phase 12: Verifying build output"

OUTPUT="$GN_OUT/starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a"

[[ -f "$OUTPUT" ]] ||
    fail "Build finished but expected library was not found: $OUTPUT"

ls -lh "$OUTPUT"

echo
echo "================================================================"
echo " BUILD COMPLETED SUCCESSFULLY"
echo "================================================================"
echo
echo "SDK: $SDK_DIR"
echo "Cobalt: $COBALT_DIR"
echo "Larboard: $LARBOARD_DIR"
echo "Output: $OUTPUT"



#!/usr/bin/env bash
set -Eeuo pipefail

###############################################################################
# Cobalt 27 / Larboard RDK ARM build
#
# Run from anywhere:
#   bash cov_build.sh
#
# All downloaded sources, SDK files and build outputs are kept inside the
# larboard Git repository workspace.
###############################################################################

# Resolve the Git repository root.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"

# Allow Git to access the GitHub Actions checkout.
git config --global --add safe.directory "$ROOT_DIR"

cd "$ROOT_DIR"

###############################################################################
# Configuration
###############################################################################

META_REPO="https://github.com/rdkcentral/meta-bolt-youtube.git"
META_BRANCH="feature/cobalt-27-from-meta-rdk-video"

COBALT_REPO="https://github.com/youtube/cobalt.git"
COBALT_TAG="27.lts.3"

DEPOT_TOOLS_REPO="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_COMMIT="2cea9cdd785391335d448773124631cb711225e2"

# Use the current repository's origin for the Larboard source checkout.
LARBOARD_REPO_URL="${LARBOARD_REPO_URL:-$(git remote get-url origin)}"
LARBOARD_COMMIT="980732e5ad863aefd2692307e08e36e3e325e07a"

META_DIR="$ROOT_DIR/meta-bolt-youtube"
SDK_DIR="$ROOT_DIR/oecore-x86_64"
SDK_SETUP="$SDK_DIR/environment-setup-armv7at2hf-neon-oe-linux-gnueabi"

DEPOT_TOOLS_DIR="$ROOT_DIR/depot_tools"
BIN_DIR="$ROOT_DIR/bin"

COBALT_ROOT="$ROOT_DIR/cobalt-build"
COBALT_DIR="$COBALT_ROOT/src"
LARBOARD_DIR="$COBALT_ROOT/larboard"

GN_OUT="$COBALT_DIR/out/rdk-arm_gold"

SDK_INSTALLER=""

###############################################################################
# Common functions
###############################################################################

log()
{
    echo
    echo "================================================================"
    echo " $1"
    echo "================================================================"
}

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 ||
        fail "Required command not found: $1"
}

clone_or_update()
{
    local repo_url="$1"
    local repo_dir="$2"
    local branch="${3:-}"

    if [ -d "$repo_dir/.git" ]; then
        echo "Reusing existing repository: $repo_dir"

        git -C "$repo_dir" remote set-url origin "$repo_url"
        git -C "$repo_dir" fetch origin

        if [ -n "$branch" ]; then
            git -C "$repo_dir" checkout "$branch"
            git -C "$repo_dir" pull --ff-only origin "$branch"
        fi
    else
        if [ -e "$repo_dir" ]; then
            fail "$repo_dir exists but is not a Git repository."
        fi

        mkdir -p "$(dirname "$repo_dir")"

        if [ -n "$branch" ]; then
            git clone --branch "$branch" "$repo_url" "$repo_dir"
        else
            git clone "$repo_url" "$repo_dir"
        fi
    fi
}

checkout_commit()
{
    local repo_dir="$1"
    local commit="$2"

    git -C "$repo_dir" cat-file -e "${commit}^{commit}" 2>/dev/null ||
        git -C "$repo_dir" fetch origin "$commit"

    git -C "$repo_dir" checkout --detach "$commit"
}

###############################################################################
# Phase 1: Check host tools
###############################################################################

log "Phase 1: Checking host tools"

for tool in git curl python3 make gcc g++; do
    require_command "$tool"
done

mkdir -p "$BIN_DIR"

###############################################################################
# Phase 2: Prepare repo launcher and Yocto source
###############################################################################

log "Phase 2: Preparing Yocto source"

if ! command -v repo >/dev/null 2>&1; then
    echo "Installing repo launcher..."

    curl -fsSL \
        https://storage.googleapis.com/git-repo-downloads/repo \
        -o "$BIN_DIR/repo"

    chmod +x "$BIN_DIR/repo"
fi

export PATH="$BIN_DIR:$PATH"

clone_or_update "$META_REPO" "$META_DIR" "$META_BRANCH"

###############################################################################
# Phase 3: Generate and install Yocto SDK if needed
###############################################################################

log "Phase 3: Preparing Yocto SDK"

if [ -f "$SDK_SETUP" ]; then
    echo "Existing Yocto SDK found. Reusing: $SDK_DIR"
else
    echo "Yocto SDK not found. Generating SDK..."

    (
        cd "$META_DIR"

        # Architect's original environment setup.
        # Yocto setup scripts may reference unset shell variables.
        set +u
        source ./setup-environment
        set -u

        echo "MACHINE=${MACHINE:-not-set}"
        echo "DISTRO=${DISTRO:-not-set}"

        bitbake cobalt27-bolt-image -c populate_sdk
    )

    SDK_INSTALLER="$(find \
        "$META_DIR/build/tmp-glibc/deploy/sdk" \
        -maxdepth 1 \
        -type f \
        -name '*toolchain*.sh' \
        -print -quit 2>/dev/null || true)"

    [ -n "$SDK_INSTALLER" ] ||
        fail "Yocto SDK installer was not generated."

    echo "Installing SDK from: $SDK_INSTALLER"

    bash "$SDK_INSTALLER" -y -d "$SDK_DIR"

    [ -f "$SDK_SETUP" ] ||
        fail "SDK installation completed, but setup file is missing."
fi

###############################################################################
# Phase 4: Prepare depot_tools
###############################################################################

log "Phase 4: Preparing depot_tools"

clone_or_update "$DEPOT_TOOLS_REPO" "$DEPOT_TOOLS_DIR"

checkout_commit "$DEPOT_TOOLS_DIR" "$DEPOT_TOOLS_COMMIT"

export PATH="$DEPOT_TOOLS_DIR:$PATH"
export DEPOT_TOOLS_UPDATE=0
export DEPOT_TOOLS_METRICS=0

(
    cd "$DEPOT_TOOLS_DIR"

    if [ -f ./bootstrap_python3 ]; then
        source ./bootstrap_python3
        bootstrap_python3
    fi
)

require_command gclient
require_command gn
require_command ninja

###############################################################################
# Phase 5: Configure and download Cobalt
###############################################################################

log "Phase 5: Preparing Cobalt"

mkdir -p "$COBALT_ROOT"

if [ ! -f "$COBALT_ROOT/.gclient" ]; then
    cat > "$COBALT_ROOT/.gclient" <<'EOF'
solutions = [
  {
    "url": "https://github.com/youtube/cobalt.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars": {
      "checkout_configuration": "small",
    },
  },
]
EOF
fi

if [ -d "$COBALT_DIR/.git" ]; then
    echo "Cobalt source already exists. Reusing it."
else
    echo "Cobalt source not found. gclient will download it."
fi

(
    cd "$COBALT_ROOT"

    gclient sync --nohooks
)

[ -d "$COBALT_DIR/.git" ] ||
    fail "Cobalt source checkout failed."

git -C "$COBALT_DIR" fetch origin --tags
git -C "$COBALT_DIR" checkout --detach "$COBALT_TAG"

###############################################################################
# Phase 6: Clone or reuse Larboard source
###############################################################################

log "Phase 6: Preparing Larboard source"

clone_or_update "$LARBOARD_REPO_URL" "$LARBOARD_DIR"

checkout_commit "$LARBOARD_DIR" "$LARBOARD_COMMIT"

###############################################################################
# Phase 7: Run Cobalt hooks
###############################################################################

log "Phase 7: Running Cobalt hooks"

(
    cd "$COBALT_ROOT"

    gclient runhooks
)

###############################################################################
# Phase 8: Configure Starboard and RDK ARM platform
###############################################################################

log "Phase 8: Configuring Starboard and RDK ARM"

STARBOARD_LINK="$COBALT_DIR/third_party/starboard"
STARBOARD_SOURCE="$LARBOARD_DIR/src/third_party/starboard"

[ -d "$STARBOARD_SOURCE" ] ||
    fail "Larboard Starboard source not found: $STARBOARD_SOURCE"

mkdir -p "$COBALT_DIR/third_party"

if [ -L "$STARBOARD_LINK" ]; then
    rm "$STARBOARD_LINK"
elif [ -e "$STARBOARD_LINK" ]; then
    fail "$STARBOARD_LINK exists and is not a symlink. Not replacing it."
fi

ln -s "$STARBOARD_SOURCE" "$STARBOARD_LINK"

echo "Starboard link:"
ls -ld "$STARBOARD_LINK"

# Create RDK ARM GN arguments.
mkdir -p "$COBALT_DIR/build/configs/rdk-arm"

cat > "$COBALT_DIR/build/configs/rdk-arm/args.gn" <<'EOF'
import("//third_party/starboard/rdk/arm/args.gn")

starboard_level_final_executable_type = "executable"
EOF

# Add rdk-arm to the supported Cobalt Starboard platforms.
python3 - "$COBALT_DIR/build/gn.py" <<'PY'
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
content = path.read_text()

pattern = r"(_COBALT_STARBOARD_PLATFORMS\s*=\s*\[)(.*?)(\])"
match = re.search(pattern, content, re.DOTALL)

if not match:
    raise SystemExit(
        "ERROR: Could not find _COBALT_STARBOARD_PLATFORMS in gn.py"
    )

platforms = match.group(2)

if re.search(r"""['"]rdk-arm['"]""", platforms):
    print("rdk-arm is already present in gn.py")
else:
    updated = (
        match.group(1)
        + platforms
        + "\n    'rdk-arm',\n"
        + match.group(3)
    )

    content = content[:match.start()] + updated + content[match.end():]
    path.write_text(content)

    print("Added rdk-arm to gn.py")
PY

###############################################################################
# Phase 9: Generate GN build files
###############################################################################

log "Phase 9: Generating GN build files"

(
    cd "$COBALT_DIR"

    source "$SDK_SETUP"
    unset PKG_CONFIG_SYSROOT_DIR

    python3 cobalt/build/gn.py --no-rbe -p rdk-arm -c gold
)

###############################################################################
# Phase 10: Build libstarboard_platform.a
###############################################################################

log "Phase 10: Building libstarboard_platform.a"

[ -d "$GN_OUT" ] ||
    fail "GN output directory not found: $GN_OUT"

(
    cd "$COBALT_DIR"

    source "$SDK_SETUP"
    unset PKG_CONFIG_SYSROOT_DIR

    ninja -C "$GN_OUT" -j4 \
        starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a
)

###############################################################################
# Phase 11: Verify build output
###############################################################################

log "Phase 11: Verifying build output"

ARCHIVE="$GN_OUT/starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a"

[ -f "$ARCHIVE" ] ||
    fail "Build completed but archive was not found: $ARCHIVE"

echo
echo "Build completed successfully."
echo
echo "Repository workspace : $ROOT_DIR"
echo "Cobalt source        : $COBALT_DIR"
echo "Larboard source      : $LARBOARD_DIR"
echo "Yocto SDK            : $SDK_DIR"
echo "Build output         : $ARCHIVE"
echo

ls -lh "$ARCHIVE"

echo
echo "All build files are located inside the larboard repository."


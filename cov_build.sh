#!/usr/bin/env bash

set -Eeuo pipefail
trap 'echo "ERROR: Build failed at line $LINENO" >&2' ERR

# Download the Yocto SDK installer from Artifactory, then install and build.
# Generated SDK, dependency checkouts and build outputs stay in this workspace.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# SDK_INSTALLER="$ROOT_DIR/oecore-x86_64-armv7at2hf-neon-toolchain-1.0.sh"
SDK_URL="https://artifactory.rdkcentral.com/artifactory/cov_larboard/oecore-x86_64-sdk.tar.gz"
SDK_DOWNLOAD_DIR="$ROOT_DIR/sdk-installer"
SDK_ARCHIVE="$SDK_DOWNLOAD_DIR/oecore-x86_64-sdk.tar.gz"
SDK_INSTALLER="$SDK_DOWNLOAD_DIR/oecore-x86_64-armv7at2hf-neon-toolchain-1.0.sh"
SDK_DIR="$ROOT_DIR/oecore-x86_64"
SDK_SETUP="$SDK_DIR/environment-setup-armv7at2hf-neon-oe-linux-gnueabi"
DEPOT_TOOLS_DIR="$ROOT_DIR/depot_tools"
COBALT_ROOT="$ROOT_DIR/cobalt-build"
COBALT_DIR="$COBALT_ROOT/src"
STARBOARD_SOURCE="$ROOT_DIR/src/third_party/starboard"
COBALT_TAG="27.lts.3"
DEPOT_TOOLS_COMMIT="2cea9cdd785391335d448773124631cb711225e2"
NINJA_JOBS="${NINJA_JOBS:-4}"

export PYTHONPYCACHEPREFIX="$COBALT_ROOT/py-cache"
export DEPOT_TOOLS_METRICS=0
export DEPOT_TOOLS_UPDATE=0
export TERM=screen
export PATH="$PATH:$DEPOT_TOOLS_DIR"

fail() {
    echo "ERROR: $1" >&2
    exit 1
}

[[ "$NINJA_JOBS" =~ ^[1-9][0-9]*$ ]] || fail "NINJA_JOBS must be a positive integer."
[[ -d "$STARBOARD_SOURCE/rdk/arm" ]] || fail "Larboard ARM Starboard source is missing."

echo "Phase 1: Install the Yocto SDK"
if [[ ! -f "$SDK_SETUP" ]]; then
    [[ ! -e "$SDK_DIR" && ! -L "$SDK_DIR" ]] ||
        fail "SDK directory exists without its environment setup file. Inspect it before reinstalling: $SDK_DIR"
    [[ -n "${ARTIFACTORY_TOKEN:-}" ]] ||
        fail "ARTIFACTORY_TOKEN is required to download the SDK."
    mkdir -p "$SDK_DOWNLOAD_DIR"
    echo "Downloading SDK from Artifactory..."
    curl --fail --location --show-error \
        --header "Authorization: Bearer $ARTIFACTORY_TOKEN" \
        "$SDK_URL" -o "$SDK_ARCHIVE"
    tar -xzf "$SDK_ARCHIVE" -C "$SDK_DOWNLOAD_DIR" \
        oecore-x86_64-armv7at2hf-neon-toolchain-1.0.sh
    [[ -f "$SDK_INSTALLER" ]] ||
        fail "SDK installer not found after extraction: $SDK_INSTALLER"
    sh "$SDK_INSTALLER" -y -d "$SDK_DIR"
else
    echo "Reusing installed SDK: $SDK_DIR"
fi

[[ -f "$SDK_SETUP" ]] || fail "SDK environment setup file is missing: $SDK_SETUP"
[[ -d "$SDK_DIR/sysroots/armv7at2hf-neon-oe-linux-gnueabi" ]] ||
    fail "SDK target sysroot is missing."
[[ -d "$SDK_DIR/sysroots/x86_64-bolt_distro-linux" ]] ||
    fail "SDK host sysroot is missing."

echo "Phase 2: Prepare depot_tools and Cobalt"
if [[ ! -e "$DEPOT_TOOLS_DIR" ]]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
fi
[[ -d "$DEPOT_TOOLS_DIR/.git" ]] || fail "depot_tools is not a Git checkout: $DEPOT_TOOLS_DIR"
git -C "$DEPOT_TOOLS_DIR" checkout --detach "$DEPOT_TOOLS_COMMIT"

mkdir -p "$COBALT_ROOT"
if [[ ! -e "$COBALT_DIR" ]]; then
    git clone https://github.com/youtube/cobalt.git "$COBALT_DIR"
fi
[[ -d "$COBALT_DIR/.git" ]] || fail "Cobalt is not a Git checkout: $COBALT_DIR"
git -C "$COBALT_DIR" checkout --detach "$COBALT_TAG"

echo "Phase 3: Initialize the SDK environment"

# Yocto setup scripts may reference unset variables.
set +u
source "$SDK_SETUP"
set -u
unset PKG_CONFIG_SYSROOT_DIR

# SDK Git may retain a certificate path from the SDK build machine.
export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
[[ -r "$SSL_CERT_FILE" ]] || fail "Host CA certificate bundle not found: $SSL_CERT_FILE"
export GIT_SSL_CAINFO="$SSL_CERT_FILE"

echo "Phase 4: Configure and sync Cobalt dependencies"
cd "$COBALT_ROOT"
gclient config -v --name=src --custom-var='checkout_configuration="small"' \
    "$(git -C src remote get-url origin)"

cd "$COBALT_DIR"
gclient sync -v --no-history -r "$(git rev-parse HEAD)" --nohooks
gclient runhooks -v

echo "Phase 5: Integrate Larboard"
STARBOARD_LINK="$COBALT_DIR/third_party/starboard"
if [[ -L "$STARBOARD_LINK" ]]; then
    [[ "$(readlink -f "$STARBOARD_LINK")" == "$(readlink -f "$STARBOARD_SOURCE")" ]] ||
        fail "Starboard symlink points elsewhere: $STARBOARD_LINK"
elif [[ -e "$STARBOARD_LINK" ]]; then
    fail "Starboard path exists and is not a symlink: $STARBOARD_LINK"
else
    ln -s "$STARBOARD_SOURCE" "$STARBOARD_LINK"
fi

mkdir -p cobalt/build/configs/rdk-arm
cat > cobalt/build/configs/rdk-arm/args.gn <<'ARGS'
import("//third_party/starboard/rdk/arm/args.gn")
starboard_level_final_executable_type="executable"
ARGS

GN_PY="$COBALT_DIR/cobalt/build/gn.py"
[[ -f "$GN_PY" ]] || fail "Cobalt GN script not found: $GN_PY"
if ! grep -q "^    'rdk-arm'," "$GN_PY"; then
    grep -q '^_COBALT_STARBOARD_PLATFORMS.*\[' "$GN_PY" ||
        fail "Could not locate the Starboard platform list in $GN_PY"
    sed -i "/^_COBALT_STARBOARD_PLATFORMS/a\\    'rdk-arm'," "$GN_PY"
fi

echo "Phase 6: Generate and build RDK ARM Starboard"
"$GN_PY" --no-rbe -p rdk-arm -c gold
TARGET="starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a"
ninja -j"$NINJA_JOBS" -C out/rdk-arm_gold "$TARGET"

echo "Phase 7: Verify build output"
OUTPUT="$COBALT_DIR/out/rdk-arm_gold/$TARGET"
[[ -f "$OUTPUT" ]] || fail "Expected library not found: $OUTPUT"
ls -lh "$OUTPUT"
echo "Build completed successfully: $OUTPUT"

g!/usr/bin/env bash
#
# build_x86_cmake.sh
# ------------------
# Compile the Cobalt / Starboard RDK source (src/) into a static library using
# CMake on an x86_64 host. This builds ONLY libstarboard_platform.a.
# It does NOT build the Thunder plugin "widget" and does NOT build any binary.
#
# REQUIREMENT
# ===========
# The RDK port sources include full-tree headers (starboard/..., third_party/...)
# that are NOT vendored in this repo. Provide a full Cobalt checkout via
# COBALT_SRC_ROOT so the compiler can find those headers.
#
# Usage:
#   ./build_x86_cmake.sh [--clean] [--install-deps] [--fetch-gstreamer]
#                        [--fetch-thunder] [-j N]
#                        [--no-ocdm] [--no-securityagent] [--no-crypto]
#                        [--no-rdkservices]
#
# Feature toggles (all ON by default, matching CMakeLists options). Use the
# --no-* flags to skip a feature whose Thunder pkg-config package is not built
# (e.g. --no-ocdm when ocdm.pc / ThunderClientLibraries is unavailable).
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration (override via environment)
# ---------------------------------------------------------------------------
# This script now lives inside larboard/ itself.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LARBOARD_DIR="${LARBOARD_DIR:-${SCRIPT_DIR}}"
SOURCE_DIR="${SOURCE_DIR:-${LARBOARD_DIR}/src}"
# Build/install output now live inside larboard/ too (gitignored; see below).
BUILD_DIR="${BUILD_DIR:-${LARBOARD_DIR}/build-x86}"
INSTALL_DIR="${INSTALL_DIR:-${LARBOARD_DIR}/install-x86}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

# External dependencies now live under larboard/dependency (alongside this script).
DEP_DIR="${DEP_DIR:-${LARBOARD_DIR}/dependency}"

# Full Cobalt source checkout providing starboard/... and third_party/... headers.
COBALT_SRC_ROOT="${COBALT_SRC_ROOT:-${DEP_DIR}/cobalt}"

# Essos (RDK EGL/input abstraction) headers dir providing essos-app.h.
# Primary source is the vendored in-tree copy under dependency/essos; other
# checkout locations are tried as a fallback. If this stays empty, CMake still
# resolves the vendored copy relative to the source tree (path-independent).
ESSOS_INCLUDE_DIR="${ESSOS_INCLUDE_DIR:-}"
if [[ -z "${ESSOS_INCLUDE_DIR}" ]]; then
  for _essos_dir in \
    "${DEP_DIR}/essos" \
    "${DEP_DIR}/essos/include" \
    "${HOME}/cobalt-rdk-x86/sources/components/opensource/westeros/essos"; do
    if [[ -f "${_essos_dir}/essos-app.h" ]]; then
      ESSOS_INCLUDE_DIR="${_essos_dir}"
      break
    fi
  done
fi

# Vendored Khronos EGL/GLES/KHR headers (essos-app.h includes <EGL/egl.h>).
# Self-contained fallback for hosts without mesa -dev packages / egl.pc.
# Overridable; defaults to the in-tree copy under dependency/khronos/include.
KHRONOS_INCLUDE_DIR="${KHRONOS_INCLUDE_DIR:-${DEP_DIR}/khronos/include}"

# Local GStreamer clone/build (used instead of the distro -dev packages).
GST_DIR="${GST_DIR:-${DEP_DIR}/gstreamer}"
GST_SRC_DIR="${GST_DIR}/src"
GST_PREFIX="${GST_PREFIX:-${GST_DIR}/install}"
GST_VERSION="${GST_VERSION:-1.24}"
GST_REPO="${GST_REPO:-https://gitlab.freedesktop.org/gstreamer/gstreamer.git}"

# Local Thunder / WPEFramework clone/build (provides WPEFramework*/ocdm/
# securityagent pkg-config packages).
THUNDER_DIR="${THUNDER_DIR:-${DEP_DIR}/thunder}"
THUNDER_SRC_DIR="${THUNDER_DIR}/src"
THUNDER_PREFIX="${THUNDER_PREFIX:-${THUNDER_DIR}/install}"
THUNDER_VERSION="${THUNDER_VERSION:-R4.4.1}"
THUNDER_GIT_BASE="${THUNDER_GIT_BASE:-https://github.com/rdkcentral}"

JOBS="$(nproc 2>/dev/null || echo 4)"
DO_CLEAN=0
DO_INSTALL_DEPS=0
DO_FETCH_GST=0
DO_FETCH_THUNDER=0

# Optional RDK features (empty = leave CMake default ON; OFF = disable).
ENABLE_OCDM=""
ENABLE_SECURITYAGENT=""
ENABLE_WPECRYPTOGRAPHY=""
ENABLE_RDKSERVICES=""

# ---------------------------------------------------------------------------
# Arg parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)          DO_CLEAN=1; shift ;;
    --install-deps)   DO_INSTALL_DEPS=1; shift ;;
    --fetch-gstreamer) DO_FETCH_GST=1; shift ;;
    --fetch-thunder)  DO_FETCH_THUNDER=1; shift ;;
    --no-ocdm)          ENABLE_OCDM=OFF; shift ;;
    --no-securityagent) ENABLE_SECURITYAGENT=OFF; shift ;;
    --no-crypto)        ENABLE_WPECRYPTOGRAPHY=OFF; shift ;;
    --no-rdkservices)   ENABLE_RDKSERVICES=OFF; shift ;;
    -j)               JOBS="$2"; shift 2 ;;
    -j*)              JOBS="${1#-j}"; shift ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

log()  { printf '\033[1;34m[build]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Host build dependencies (Debian/Ubuntu package names)
# ---------------------------------------------------------------------------
install_deps() {
  log "Installing host build dependencies (requires sudo)..."
  # sudo apt-get update
  # sudo apt-get install -y \
   # build-essential \
   # cmake \
    # ninja-build \
    # pkg-config \
    # git \
    # ca-certificates \
    # zlib1g-dev \
    # libssl-dev \
    # flex \
    # bison \
    # libglib2.0-dev \
    # libgstreamer1.0-dev \
    # libgstreamer-plugins-base1.0-dev \
    # libgles2-mesa-dev \
    # libegl1-mesa-dev
  # NOTE: Thunder/WPEFramework (WPEFrameworkCore, WebSocket, Cryptography, ocdm,
  # securityagent) are NOT distro packages. Build/install them separately so
  # their .pc files are visible to pkg-config (PKG_CONFIG_PATH).
}

# ---------------------------------------------------------------------------
# Toolchain / dependency checks
# ---------------------------------------------------------------------------
check_tools() {
  command -v cmake     >/dev/null || die "cmake not found. Run with --install-deps."
  command -v pkg-config>/dev/null || die "pkg-config not found. Run with --install-deps."
  local gen="Unix Makefiles"
  if command -v ninja >/dev/null; then gen="Ninja"; fi
  echo "$gen"
}

check_cobalt_src() {
  if [[ ! -f "${COBALT_SRC_ROOT}/starboard/event.h" ]]; then
    warn "COBALT_SRC_ROOT does not look like a Cobalt checkout:"
    warn "  ${COBALT_SRC_ROOT}"
    warn "Missing starboard/event.h. Set COBALT_SRC_ROOT to a full Cobalt tree."
    warn "The CMake configure step will fail without it."
  else
    log "Using Cobalt source root: ${COBALT_SRC_ROOT}"
  fi
}

# ---------------------------------------------------------------------------
# GStreamer: clone + build into the dependency folder (no root/apt needed).
# ---------------------------------------------------------------------------
gst_pc_dirs() {
  # Emit the pkgconfig dirs of the local GStreamer install (multiarch + plain).
  local d
  for d in "${GST_PREFIX}/lib/$(uname -m)-linux-gnu/pkgconfig" \
           "${GST_PREFIX}/lib/pkgconfig" \
           "${GST_PREFIX}/lib64/pkgconfig"; do
    [[ -d "${d}" ]] && printf '%s:' "${d}"
  done
  return 0
}

fetch_gstreamer() {
  command -v meson >/dev/null || die "meson not found (pip install meson, or apt install meson)."
  command -v ninja >/dev/null || die "ninja not found (apt install ninja-build)."
  command -v flex  >/dev/null || die "flex not found (apt install flex). Needed by gstreamer/gst/parse."
  command -v bison >/dev/null || die "bison not found (apt install bison). Needed by gstreamer/gst/parse."

  if [[ ! -d "${GST_SRC_DIR}/.git" ]]; then
    log "Cloning GStreamer ${GST_VERSION} into ${GST_SRC_DIR}"
    mkdir -p "${GST_DIR}"
    git clone --depth 1 --branch "${GST_VERSION}" "${GST_REPO}" "${GST_SRC_DIR}"
  else
    log "GStreamer clone already present: ${GST_SRC_DIR}"
  fi

  log "Building GStreamer (core + plugins-base) into ${GST_PREFIX}"
  meson setup "${GST_SRC_DIR}/builddir" "${GST_SRC_DIR}" \
    --prefix="${GST_PREFIX}" \
    --buildtype=release \
    -Dgood=disabled -Dbad=disabled -Dugly=disabled \
    -Dlibav=disabled -Ddevtools=disabled -Dexamples=disabled \
    -Dtests=disabled -Dgst-examples=disabled -Dpython=disabled \
    -Dintrospection=disabled -Ddoc=disabled \
    --reconfigure || \
  meson setup "${GST_SRC_DIR}/builddir" "${GST_SRC_DIR}" \
    --prefix="${GST_PREFIX}" \
    --buildtype=release \
    -Dgood=disabled -Dbad=disabled -Dugly=disabled \
    -Dlibav=disabled -Ddevtools=disabled -Dexamples=disabled \
    -Dtests=disabled -Dgst-examples=disabled -Dpython=disabled \
    -Dintrospection=disabled -Ddoc=disabled
  ninja -C "${GST_SRC_DIR}/builddir" install
  log "GStreamer installed. pkgconfig: $(gst_pc_dirs)"
}

# ---------------------------------------------------------------------------
# Thunder / WPEFramework: clone + build into the dependency folder.
# Provides WPEFrameworkCore/Definitions/WebSocket/Cryptography, ocdm,
# securityagent pkg-config packages.
# ---------------------------------------------------------------------------
thunder_pc_dirs() {
  local d
  for d in "${THUNDER_PREFIX}/lib/$(uname -m)-linux-gnu/pkgconfig" \
           "${THUNDER_PREFIX}/lib/pkgconfig" \
           "${THUNDER_PREFIX}/lib64/pkgconfig"; do
    [[ -d "${d}" ]] && printf '%s:' "${d}"
  done
  return 0
}

# ---------------------------------------------------------------------------
# Self-heal relocated pkg-config files.
# GStreamer (meson) and Thunder (cmake) bake an ABSOLUTE 'prefix=' into every
# .pc file at install time. When the dependency/ tree is built on one machine
# and copied to another path (e.g. local PC -> DEV VM), pkg-config then emits
# -I flags pointing at the OLD, non-existent directory, producing errors like
# "fatal error: core/JSON.h" or "gst/gst.h: No such file or directory" even
# though CMake's pkg_check_modules() succeeded.
#
# The repair now lives in CMakeLists.txt (it stages prefix-healed copies of the
# .pc files into the build dir and puts them first on PKG_CONFIG_PATH), so it
# works from ANY path and even when configuring without this wrapper. Nothing
# to do here beyond exposing the local pkgconfig dirs below.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Verify system headers that have NO pkg-config module in this build but are
# still #included transitively (EGL/egl.h and GLESv2 come in via essos-app.h).
# On a fresh DEV VM these are missing until libegl1-mesa-dev / libgles2-mesa-dev
# are installed; fail early with an actionable message instead of deep in the
# compile step.
# ---------------------------------------------------------------------------
check_system_headers() {
  # The vendored Khronos headers satisfy these includes without any system
  # package; only warn if they are ALSO absent (e.g. the fallback was deleted).
  local inc=""
  [[ -d "${KHRONOS_INCLUDE_DIR}" ]] && inc="-I${KHRONOS_INCLUDE_DIR}"
  local missing=()
  echo '#include <EGL/egl.h>' | ${CXX:-c++} -E -x c++ - ${inc} >/dev/null 2>&1 \
    || missing+=("EGL/egl.h (libegl1-mesa-dev)")
  echo '#include <GLES2/gl2.h>' | ${CXX:-c++} -E -x c++ - ${inc} >/dev/null 2>&1 \
    || missing+=("GLES2/gl2.h (libgles2-mesa-dev)")
  if [[ ${#missing[@]} -gt 0 ]]; then
    warn "Missing headers required by essos-app.h (not on system path and no"
    warn "vendored copy at ${KHRONOS_INCLUDE_DIR}):"
    for m in "${missing[@]}"; do warn "  - ${m}"; done
    warn "Fix by either installing the packages:"
    warn "  sudo apt-get install -y libegl1-mesa-dev libgles2-mesa-dev"
    warn "or restoring dependency/khronos/include (self-contained fallback)."
  else
    [[ -n "${inc}" ]] && log "EGL/GLES headers: using vendored ${KHRONOS_INCLUDE_DIR}"
  fi
}

_thunder_clone() {
  local name="$1"
  if [[ ! -d "${THUNDER_SRC_DIR}/${name}/.git" ]]; then
    log "Cloning ${name} (${THUNDER_VERSION})"
    git clone --depth 1 --branch "${THUNDER_VERSION}" \
      "${THUNDER_GIT_BASE}/${name}.git" "${THUNDER_SRC_DIR}/${name}"
  else
    log "${name} already cloned"
  fi
}

# Thunder installs the securityagent client lib as 'WPEFrameworkSecurityAgent.pc'
# but the RDK port looks up the pkg-config module 'securityagent'. pkg-config
# resolves modules by filename, so drop an alias .pc alongside the original.
_thunder_alias_pc() {
  local src_name="$1" alias_name="$2" d
  for d in "${THUNDER_PREFIX}/lib/$(uname -m)-linux-gnu/pkgconfig" \
           "${THUNDER_PREFIX}/lib/pkgconfig" \
           "${THUNDER_PREFIX}/lib64/pkgconfig"; do
    if [[ -f "${d}/${src_name}.pc" && ! -f "${d}/${alias_name}.pc" ]]; then
      cp "${d}/${src_name}.pc" "${d}/${alias_name}.pc"
      log "Aliased pkg-config: ${alias_name}.pc -> ${src_name}.pc"
    fi
  done
}

_thunder_build() {
  local name="$1"; shift
  log "Building ${name}"
  cmake -S "${THUNDER_SRC_DIR}/${name}" -B "${THUNDER_SRC_DIR}/${name}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${THUNDER_PREFIX}" \
    -DCMAKE_PREFIX_PATH="${THUNDER_PREFIX}" \
    "$@"
  cmake --build "${THUNDER_SRC_DIR}/${name}/build" -j "${JOBS}"
  cmake --install "${THUNDER_SRC_DIR}/${name}/build"
}

fetch_thunder() {
  command -v cmake >/dev/null || die "cmake not found. Run with --install-deps."
  mkdir -p "${THUNDER_SRC_DIR}"

  # Thunder's JSON/proxy-stub code generators (ThunderTools) need these Python
  # modules; missing jsonref surfaces as "JsonGenerator generator failed".
  if command -v pip3 >/dev/null; then
    pip3 install --break-system-packages -q jsonref jsonschema || \
      warn "Could not auto-install jsonref/jsonschema; install them manually."
  else
    warn "pip3 not found; Thunder generators need the 'jsonref' Python module."
  fi

  _thunder_clone "ThunderTools"
  _thunder_clone "Thunder"
  _thunder_clone "ThunderInterfaces"
  _thunder_clone "ThunderClientLibraries"

  # ThunderClientLibraries/ocdm needs gstreamer-1.0 via pkg-config; expose the
  # local GStreamer (and already-installed Thunder packages) to this build.
  local _thunder_pc="$(gst_pc_dirs)$(thunder_pc_dirs)"
  if [[ -n "${_thunder_pc}" ]]; then
    export PKG_CONFIG_PATH="${_thunder_pc}${PKG_CONFIG_PATH:-}"
  fi

  # 1) Code-generator tools, 2) core, 3) interfaces, 4) client libs (ocdm etc).
  _thunder_build "ThunderTools"
  _thunder_build "Thunder" \
    -DBUILD_TYPE=Release \
    -DBINDING="127.0.0.1" -DPORT="55555" \
    -DCRYPTOGRAPHY=ON
  _thunder_build "ThunderInterfaces"
  _thunder_build "ThunderClientLibraries" \
    -DCDMI=ON \
    -DCDMI_ADAPTER_IMPLEMENTATION=gstreamer \
    -DSECURITYAGENT=ON \
    -DCRYPTOGRAPHY=ON \
    -DCRYPTOGRAPHY_IMPLEMENTATION=OpenSSL

  # RDK port expects module 'securityagent'; Thunder ships 'WPEFrameworkSecurityAgent'.
  _thunder_alias_pc "WPEFrameworkSecurityAgent" "securityagent"

  log "Thunder installed. pkgconfig: $(thunder_pc_dirs)"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
[[ ${DO_INSTALL_DEPS}  -eq 1 ]] && install_deps
[[ ${DO_FETCH_GST}     -eq 1 ]] && fetch_gstreamer
[[ ${DO_FETCH_THUNDER} -eq 1 ]] && fetch_thunder

GENERATOR="$(check_tools)"
check_cobalt_src

# Prepend local GStreamer/Thunder installs to PKG_CONFIG_PATH if present, so the
# CMake pkg_check_modules() calls resolve without distro/system packages.
EXTRA_PC="$(gst_pc_dirs)$(thunder_pc_dirs)"
if [[ -n "${EXTRA_PC}" ]]; then
  export PKG_CONFIG_PATH="${EXTRA_PC}${PKG_CONFIG_PATH:-}"
  log "Using local pkgconfig: ${EXTRA_PC}"
fi

# Repair of stale .pc prefixes is handled inside CMakeLists.txt (path- and
# entry-point-independent). Here we only verify the non-pkg-config system
# headers essos-app.h depends on.
check_system_headers

[[ -d "${SOURCE_DIR}" ]] || die "Source dir not found: ${SOURCE_DIR}"

if [[ ${DO_CLEAN} -eq 1 && -d "${BUILD_DIR}" ]]; then
  log "Cleaning ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

# Only pass a -D flag when the user explicitly disabled a feature; otherwise the
# CMakeLists default (ON) applies.
FEATURE_FLAGS=()
[[ -n "${ENABLE_OCDM}" ]]            && FEATURE_FLAGS+=(-DRDK_ENABLE_OCDM="${ENABLE_OCDM}")
[[ -n "${ENABLE_SECURITYAGENT}" ]]  && FEATURE_FLAGS+=(-DRDK_ENABLE_SECURITYAGENT="${ENABLE_SECURITYAGENT}")
[[ -n "${ENABLE_WPECRYPTOGRAPHY}" ]] && FEATURE_FLAGS+=(-DRDK_ENABLE_WPECRYPTOGRAPHY="${ENABLE_WPECRYPTOGRAPHY}")
[[ -n "${ENABLE_RDKSERVICES}" ]]   && FEATURE_FLAGS+=(-DRDK_ENABLE_RDKSERVICES_API="${ENABLE_RDKSERVICES}")

log "Configuring (generator: ${GENERATOR}, type: ${BUILD_TYPE})"
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCOBALT_SRC_ROOT="${COBALT_SRC_ROOT}" \
  -DESSOS_INCLUDE_DIR="${ESSOS_INCLUDE_DIR}" \
  -DKHRONOS_INCLUDE_DIR="${KHRONOS_INCLUDE_DIR}" \
  "${FEATURE_FLAGS[@]}"

log "Building libstarboard_platform.a with ${JOBS} jobs"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

log "Installing to ${INSTALL_DIR}"
cmake --install "${BUILD_DIR}"

log "Done. Static library under: ${INSTALL_DIR}/lib"

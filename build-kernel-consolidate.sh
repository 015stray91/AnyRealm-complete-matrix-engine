#!/bin/bash
# Complete genevn kernel build with CONSOLIDATION VARIANT
# Uses: build.config.msm.parrot -> sets VARIANT=consolidate (DEFAULT)
#
# CONSOLIDATION BUILD STACK (Bottom to Top):
# ==========================================
# ROOT:
#   1. build.config.msm.parrot
#      ├── VARIANT=consolidate (DEFAULT)
#      ├── VARIANTS=(consolidate gki)
#      ├── sources: common/build.config.common
#      ├── sources: common/build.config.aarch64
#      ├── sources: build.config.msm.common
#      ├── sources: build.config.msm.gki  <-- triggers consolidate logic
#      └── sources: build.config.moto
#   2. modules.list.msm.parrot (94 modules)
#   3. modules.vendor_blocklist.msm.parrot
#
# CONSOLIDATION CONFIG:
#   4. build.config.gki_consolidate.aarch64
#   5. arch/arm64/configs/consolidate.fragment (debug + kunit)
#   6. arch/arm64/configs/gki_defconfig (BASE)
#
# DEVICE SPECIFIC:
#   7. arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config
#   8. arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config (FINAL)

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(dirname "$SCRIPT_DIR")

echo "========================================"
echo "Building Genevn Kernel - CONSOLIDATION"
echo "========================================"
echo ""
echo "VARIANT: consolidate (DEFAULT from build.config.msm.parrot)"
echo ""
echo "BUILD STACK (Bottom to Top):"
echo ""
echo "ROOT LEVEL:"
echo "  1. build.config.msm.parrot"
echo "     VARIANTS=(consolidate gki)"
echo "     VARIANT=consolidate (DEFAULT)"
echo "     Includes: common, aarch64, msm.common, msm.gki, moto"
echo "  2. modules.list.msm.parrot (94 parrot modules)"
echo "  3. modules.vendor_blocklist.msm.parrot"
echo ""
echo "CONSOLIDATION CONFIG:"
echo "  4. build.config.gki_consolidate.aarch64"
echo "  5. arch/arm64/configs/consolidate.fragment (debug+kunit)"
echo "  6. arch/arm64/configs/gki_defconfig (BASE)"
echo ""
echo "DEVICE SPECIFIC:"
echo "  7. arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config"
echo "  8. arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config (FINAL)"
echo ""

# Load environment
source "${SCRIPT_DIR}/environment.config"

# Build log
BUILD_LOG="${DIST_DIR}/build.log"
mkdir -p "${DIST_DIR}"
exec 1> >(tee -a "$BUILD_LOG")
exec 2>&1

echo "Start Time: $(date)"
echo ""

################################################################################
# STEP 0: Verify ROOT level files exist
################################################################################
echo "[0/9] Verifying consolidation build stack..."
cd "${ROOT_DIR}"

echo "  Checking ROOT level configs:"
if [ -f "build.config.msm.parrot" ]; then
    VARIANT_DEFAULT=$(grep 'VARIANT=consolidate' build.config.msm.parrot)
    echo "    ✓ build.config.msm.parrot (VARIANT: consolidate)"
else
    echo "    ✗ build.config.msm.parrot NOT FOUND"
    exit 1
fi

if [ -f "modules.list.msm.parrot" ]; then
    MOD_COUNT=$(grep -c "^" modules.list.msm.parrot || true)
    echo "    ✓ modules.list.msm.parrot ($MOD_COUNT modules)"
else
    echo "    ✗ modules.list.msm.parrot NOT FOUND"
    exit 1
fi

if [ -f "modules.vendor_blocklist.msm.parrot" ]; then
    BLOCK_COUNT=$(grep -c "^blocklist" modules.vendor_blocklist.msm.parrot || true)
    echo "    ✓ modules.vendor_blocklist.msm.parrot ($BLOCK_COUNT blocklisted)"
else
    echo "    ✗ modules.vendor_blocklist.msm.parrot NOT FOUND"
    exit 1
fi

echo "  Checking CONSOLIDATION config:"
if [ -f "build.config.gki_consolidate.aarch64" ]; then
    echo "    ✓ build.config.gki_consolidate.aarch64"
else
    echo "    ✗ build.config.gki_consolidate.aarch64 NOT FOUND"
    exit 1
fi

if [ -f "arch/arm64/configs/consolidate.fragment" ]; then
    echo "    ✓ arch/arm64/configs/consolidate.fragment"
else
    echo "    ✗ arch/arm64/configs/consolidate.fragment NOT FOUND"
    exit 1
fi

echo "  Checking arch/arm64/configs/ files:"
if [ -f "arch/arm64/configs/gki_defconfig" ]; then
    echo "    ✓ arch/arm64/configs/gki_defconfig (BASE)"
else
    echo "    ✗ arch/arm64/configs/gki_defconfig NOT FOUND"
    exit 1
fi

if [ -f "arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config" ]; then
    echo "    ✓ arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config"
else
    echo "    ✗ arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config NOT FOUND"
    exit 1
fi

if [ -f "arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config" ]; then
    echo "    ✓ arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config (YOUR DEVICE)"
else
    echo "    ✗ arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config NOT FOUND"
    exit 1
fi

echo "✓ All consolidation build configs found"
echo ""

################################################################################
# STEP 1: Validate
################################################################################
echo "[1/9] Validating environment..."
if ! bash "${SCRIPT_DIR}/validate-environment.sh" > /dev/null 2>&1; then
    echo "ERROR: Environment validation failed"
    exit 1
fi
echo "✓ Environment OK"
echo ""

################################################################################
# STEP 2: Prepare directories
################################################################################
echo "[2/9] Preparing build directories..."
mkdir -p "${OUT_DIR}" "${DIST_DIR}" "${MODULES_STAGING_DIR}"
rm -f "${OUT_DIR}/.config"
echo "✓ Build directories ready"
echo ""

################################################################################
# STEP 3: Apply ROOT level build config (consolidate variant)
################################################################################
echo "[3/9] Loading ROOT level configuration (consolidate variant)..."
echo "  From: build.config.msm.parrot"
echo "  VARIANT=consolidate (DEFAULT)"
echo "  This sources:"
echo "    - common/build.config.common"
echo "    - common/build.config.aarch64"
echo "    - build.config.msm.common"
echo "    - build.config.msm.gki (triggers consolidate logic)"
echo "    - build.config.moto"

cd "${ROOT_DIR}"
export VARIANT=consolidate
export MSM_ARCH=parrot
export MODULES_LIST="${ROOT_DIR}/modules.list.msm.parrot"
export MODULES_BLOCKLIST="${ROOT_DIR}/modules.vendor_blocklist.msm.parrot"
export VENDOR_DLKM_MODULES_BLOCKLIST="${ROOT_DIR}/modules.vendor_blocklist.msm.parrot"

echo "✓ ROOT consolidate config settings applied"
echo ""

################################################################################
# STEP 4: Merge consolidation config fragment
################################################################################
echo "[4/9] Merging consolidation configuration..."
echo "  File: arch/arm64/configs/consolidate.fragment"
echo "  Adds: debug features, kunit, testing framework"

# Start with GKI base
echo "  Loading: arch/arm64/configs/gki_defconfig"
make ARCH=${ARCH} gki_defconfig O="${OUT_DIR}" > /dev/null 2>&1

# Merge consolidation fragment
echo "  Merging: consolidate.fragment"
python3 scripts/kconfig/merge_config.sh \
    -m -r -y \
    -O "${OUT_DIR}" \
    "${OUT_DIR}/.config" \
    "arch/arm64/configs/consolidate.fragment" 2>&1 | grep -v "^#" | head -3

echo "✓ Consolidation fragment merged"
echo ""

################################################################################
# STEP 5: Merge device configs (parrot + genevn)
################################################################################
echo "[5/9] Merging device-specific configurations..."
echo "  Stacking (bottom to top):"
echo "    1. moto-parrot-gki.config (Parrot board)"
echo "    2. moto-parrot-genevn.config (YOUR DEVICE - FINAL)"

# Layer 1: Motorola Parrot board config
echo "  Merging: arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config"
python3 scripts/kconfig/merge_config.sh \
    -m -r -y \
    -O "${OUT_DIR}" \
    "${OUT_DIR}/.config" \
    "arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config" 2>&1 | grep -v "^#" | head -3

# Layer 2: YOUR genevn custom config (FINAL)
echo "  Merging: arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config"
python3 scripts/kconfig/merge_config.sh \
    -m -r -y \
    -O "${OUT_DIR}" \
    "${OUT_DIR}/.config" \
    "arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config" 2>&1 | grep -v "^#" | head -3

# Finalize config
echo "  Finalizing with olddefconfig..."
make ARCH=${ARCH} \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    CC="${CC}" \
    O="${OUT_DIR}" \
    olddefconfig > /dev/null 2>&1

echo "✓ Device config merged (genevn settings as final layer)"
echo ""

################################################################################
# STEP 6: Compile kernel
################################################################################
echo "[6/9] Compiling kernel image..."
cd "${ROOT_DIR}"
make ARCH=${ARCH} \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    CC="${CC}" \
    AR="${AR}" \
    LLVM=1 \
    LLVM_IAS=1 \
    O="${OUT_DIR}" \
    ${MAKE_JOBS} \
    Image 2>&1 | tail -15

if [ ! -f "${OUT_DIR}/arch/arm64/boot/Image" ]; then
    echo "ERROR: Kernel image not built"
    exit 1
fi
echo "✓ Kernel compiled"
echo ""

################################################################################
# STEP 7: Compile device trees
################################################################################
echo "[7/9] Compiling device trees..."
make ARCH=${ARCH} \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    CC="${CC}" \
    O="${OUT_DIR}" \
    ${MAKE_JOBS} \
    dtbs > /dev/null 2>&1

DTB_COUNT=$(find "${OUT_DIR}/arch/arm64/boot/dts" -name "*.dtb" 2>/dev/null | wc -l)
echo "✓ DTBs compiled ($DTB_COUNT files)"
echo ""

################################################################################
# STEP 8: Compile modules
################################################################################
echo "[8/9] Compiling modules (consolidate variant + parrot modules)..."
echo "  Using modules.list.msm.parrot for ramdisk modules"
echo "  Using modules.vendor_blocklist.msm.parrot for blocklist"

make ARCH=${ARCH} \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    CC="${CC}" \
    O="${OUT_DIR}" \
    INSTALL_MOD_PATH="${MODULES_STAGING_DIR}" \
    MODULES_LIST="${MODULES_LIST}" \
    MODULES_BLOCKLIST="${MODULES_BLOCKLIST}" \
    ${MAKE_JOBS} \
    modules modules_install > /dev/null 2>&1

MOD_COUNT=$(find "${MODULES_STAGING_DIR}" -name "*.ko" 2>/dev/null | wc -l)
echo "✓ Modules compiled ($MOD_COUNT files)"
echo ""

################################################################################
# STEP 9: Collect artifacts
################################################################################
echo "[9/9] Collecting build artifacts..."

cp "${OUT_DIR}/arch/arm64/boot/Image" "${DIST_DIR}/Image" 2>/dev/null || true
cp "${OUT_DIR}/arch/arm64/boot/Image.gz" "${DIST_DIR}/Image.gz" 2>/dev/null || true
find "${OUT_DIR}/arch/arm64/boot/dts" -name "*.dtb" -o -name "*.dtbo" 2>/dev/null | xargs -I {} cp {} "${DIST_DIR}/" 2>/dev/null || true
cp "${OUT_DIR}/.config" "${DIST_DIR}/.config.final" 2>/dev/null || true
cp "${OUT_DIR}/System.map" "${DIST_DIR}/" 2>/dev/null || true
cp "${ROOT_DIR}/modules.list.msm.parrot" "${DIST_DIR}/" 2>/dev/null || true
cp "${ROOT_DIR}/modules.vendor_blocklist.msm.parrot" "${DIST_DIR}/" 2>/dev/null || true

if [ -d "${MODULES_STAGING_DIR}/lib/modules" ]; then
    (cd "${MODULES_STAGING_DIR}" && tar czf "${DIST_DIR}/modules.tar.gz" lib/ 2>/dev/null || true)
fi

echo "✓ Artifacts collected"
echo ""

################################################################################
# Build Summary
################################################################################
echo "========================================"
echo "Build Complete!"
echo "========================================"
echo ""
echo "Device Info:"
echo "  Device: Motorola Moto G Style 2023 5G (genevn)"
echo "  Chipset: Snapdragon 6450 (SM6450 / Parrot)"
echo "  Build Type: CONSOLIDATION + DEVICE CONFIG"
echo "  Status: SUCCESS ✓"
echo ""
echo "Build Configuration Stack:"
echo ""
echo "ROOT LEVEL (build.config.msm.parrot):"
echo "  VARIANT=consolidate (DEFAULT)"
echo "  modules.list.msm.parrot (94 modules)"
echo "  modules.vendor_blocklist.msm.parrot"
echo ""
echo "CONSOLIDATION CONFIG:"
echo "  build.config.gki_consolidate.aarch64"
echo "  arch/arm64/configs/consolidate.fragment"
echo "  + debug features, kunit, testing framework"
echo ""
echo "DEVICE SPECIFIC:"
echo "  arch/arm64/configs/vendor/ext_config/moto-parrot-gki.config"
echo "  arch/arm64/configs/vendor/ext_config/moto-parrot-genevn.config (FINAL)"
echo ""
echo "Build Artifacts (${DIST_DIR}/):"
ls -lh "${DIST_DIR}" 2>/dev/null | tail -n +2 | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Build Log: ${BUILD_LOG}"
echo "========================================"

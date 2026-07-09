#!/bin/bash
# ===================================================================
#      KOTOAMATSUKAMI-CORE : MANGEKYOU MANIFESTATION DISPATCHER
# OPERATOR: stray91 / slm015 | PROFILE: ReflectionOS12-5.10
# TARGET REPO: 015stray91/KOTOAMATSUKAMI-CORE
# ===================================================================

set -e

# === 📂 CORE REPOSITORY TRACKING PATHS ===
KERNEL_SRC="../kernel-msm-1"
OUT_MODULES="./compiled_variants"

mkdir -p "$OUT_MODULES"

echo "=========================================================="
echo "    AWAKENING THE MANGEKYOU SHARINGAN MODULE COMPILER      "
echo "=========================================================="

# Read the targeted manifestation input from your CI workflow options
SHARINGAN_EYE=${1:-"SHISUI"}

case "$SHARINGAN_EYE" in
    "SHISUI")
        echo "[>] Awakening Manifestation: KOTOAMATSUKAMI..."
        # The ultimate hidden genjutsu. Merges base SUSFS hooks and Patch 70 Safety.
        MOD_NAME="kotoamatsukami"
        DEFINES="-DCONFIG_KSU_SUSFS=y -DCONFIG_KSU_SAFETY=y"
        ;;
        
    "OBITO")
        echo "[>] Awakening Manifestation: KAMUI..."
        # Spatial-temporal distortion. Forces ZeroMount Patch 60/65 VFS and ADBD path-redirection.
        MOD_NAME="kamui"
        DEFINES="-DCONFIG_ZEROMOUNT_ADBD=y -DCONFIG_SECCOMP=y"
        ;;
        
    "ITACHI")
        echo "[>] Awakening Manifestation: TSUKUYOMI..."
        # Total illusion control. Locks down strict directory-node masking and process isolation.
        MOD_NAME="tsukuyomi"
        DEFINES="-DCONFIG_KSU_SUSFS=y -DCONFIG_KSU_SAFETY=y -DCONFIG_STRICT_ISOLATION=y"
        ;;
        
    "MADARA")
        echo "[>] Awakening Manifestation: SUSANOO..."
        # The ultimate armored shield. Combines all upstreams, 5 patches, and overclocked thermal tables.
        MOD_NAME="susanoo"
        DEFINES="-DCONFIG_KSU_SUSFS=y -DCONFIG_ZEROMOUNT_ADBD=y -DCONFIG_KSU_SAFETY=y -DCONFIG_KIO_FUSE=y"
        ;;
        
    *)
        echo "[!] Unknown Sharingan manifestation path. Dropping execution loop."
        exit 1
        ;;
esac

# === 🛠️ LLVM NATIVE COMPILATION ENGINE PASS ===
echo "[*] Directing LLVM Clang-12 cores over target track: ${MOD_NAME}.ko"

export ARCH=arm64
export CROSS_COMPILE="aarch64-linux-gnu-"

# Compiles the standalone variant directly out of your patch_engine tree
make -C "$KERNEL_SRC" M="$(pwd)/patch_engine" \
    EXTRA_CFLAGS="$DEFINES -DMODULE_VARIANT_NAME=\\\"${MOD_NAME}\\\"" \
    modules

if [ -f "patch_engine/anyrealm_fuse.ko" ]; then
    mv -f patch_engine/anyrealm_fuse.ko "${OUT_MODULES}/${MOD_NAME}.ko"
    echo "=========================================================="
    echo "[+] SUCCESS: EYEBALL MATRIX LOCKED! ${MOD_NAME}.ko SEALED NATIVELY."
    echo "=========================================================="
else
    echo "[!] Critical Error: Manifestation compilation pass failed."
    exit 1
fi

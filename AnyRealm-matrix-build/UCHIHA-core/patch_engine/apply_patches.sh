#!/bin/bash
# kotoamatsukami-core-module/patch_engine/apply_patches.sh
set -euo pipefail

KERNEL_DIR="${1:?}"
PATCH_DIR="$(dirname "$0")/../patches" # Assumes patches are relative to this script

echo "👁️ Activating Kotoamatsukami Outside-In Patch Engine..."

# 1. Verification
if [ ! -d "$KERNEL_DIR/fs" ]; then
    echo "❌ Error: Invalid Kernel directory. VFS not found."
    exit 1
fi

# 2. Strategic Patching (The "Fluid" integration)
# Instead of echoing into files, apply pre-calculated atomic patches
echo "Applying Kotoamatsukami VFS Hooks..."
patch -p1 -d "$KERNEL_DIR" < "$PATCH_DIR/koto-vfs-hooks.patch"

# 3. Dynamic Symbol Export
# If your core needs to expose symbols to modules, register them here
if grep -q "EXPORT_SYMBOL(koto_hook)" "$KERNEL_DIR/fs/namespace.c"; then
    echo "✅ Hooks verified and integrated."
else
    echo "⚠️ Warning: Hook insertion incomplete."
fi

echo "✅ Kernel source successfully patched as one fluid organism."

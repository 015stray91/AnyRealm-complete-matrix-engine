#!/bin/bash
# Apply per-device Samsung SUSFS fix patches (KDP ifdef conflict resolution)
# These patches fix Samsung's KDP modifications in fs/namespace.c, fs/proc/base.c, fs/exec.c
# that conflict with SUSFS patch hunks

DEVICE_KEY="$1"
KERNEL_COMMON="$2"
FIX_DIR="$3"

[ -d "$KERNEL_COMMON" ] || { echo "apply-samsung-fixes: kernel dir not found: $KERNEL_COMMON"; exit 1; }

if [ ! -d "$FIX_DIR" ]; then
  echo "apply-samsung-fixes: no fix patches for $DEVICE_KEY (dir not found: $FIX_DIR)"
  exit 0
fi

cd "$KERNEL_COMMON" || exit 1

APPLIED=0
for patch in "$FIX_DIR"/*.patch; do
  [ -f "$patch" ] || continue
  PATCH_NAME=$(basename "$patch")
  if patch -p1 -F3 --no-backup-if-mismatch < "$patch"; then
    echo "apply-samsung-fixes: applied $PATCH_NAME"
    APPLIED=$((APPLIED + 1))
  else
    echo "apply-samsung-fixes: FAILED to apply $PATCH_NAME"
    exit 1
  fi
done

echo "apply-samsung-fixes: $APPLIED patches applied for $DEVICE_KEY"

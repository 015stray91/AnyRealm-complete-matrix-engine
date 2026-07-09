#!/system/bin/sh
# kotoamatsukami-core-module/post-fs-data.sh

MODDIR=${0%/*}

# Activate the Kotoamatsukami VFS Overlays
# This triggers the kernel-side hooks we patched into fs/namespace.c
$MODDIR/system/bin/koto-core --mount-genjutsu

# Map the VPS RAM to the local system memory bridge
$MODDIR/system/bin/koto-core --fuse-vps-ram

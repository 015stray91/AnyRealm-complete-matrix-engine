#!/bin/bash
set -euo pipefail

# Assemble defconfig for OEM kernel builds. Appends KSU/SUSFS/ZeroMount/KPM
# toggles to gki_defconfig and validates symbols against Kconfig files.

DEFCONFIG="${1:?Usage: assemble-oem-defconfig.sh <defconfig_path> [ksu_variant]}"
KSU_VARIANT="${2:-SukiSU}"

ADD_SUSFS="${ADD_SUSFS:-true}"
ADD_ZEROMOUNT="${ADD_ZEROMOUNT:-true}"
ADD_KPM="${ADD_KPM:-false}"
ADD_ZRAM="${ADD_ZRAM:-false}"
ADD_RUST_BUILD="${ADD_RUST_BUILD:-false}"
KCONFIG_SEARCH_DIR="${KCONFIG_SEARCH_DIR:-}"

echo "assemble-oem-defconfig: variant=${KSU_VARIANT} susfs=${ADD_SUSFS} zeromount=${ADD_ZEROMOUNT} kpm=${ADD_KPM} zram=${ADD_ZRAM} rust=${ADD_RUST_BUILD}"

if [ ! -f "$DEFCONFIG" ]; then
  echo "::error::assemble-oem-defconfig: defconfig not found: ${DEFCONFIG}" >&2
  exit 1
fi

# Base KSU configs
cat >> "$DEFCONFIG" << 'EOF'
CONFIG_KSU=y
CONFIG_KSU_MANUAL_HOOK=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
CONFIG_LOG_BUF_SHIFT=20
CONFIG_MODULES=y
CONFIG_MODULE_FORCE_LOAD=y
CONFIG_MODULE_UNLOAD=y
CONFIG_MODULE_FORCE_UNLOAD=y
CONFIG_MODVERSIONS=y
CONFIG_TRIM_UNUSED_KSYMS=n
EOF

if [ "$ADD_SUSFS" != "true" ]; then
  echo "# CONFIG_KSU_SUSFS is not set" >> "$DEFCONFIG"
fi

if [ "$ADD_SUSFS" = "true" ]; then
  cat >> "$DEFCONFIG" << 'EOF'
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SUS_KSTAT_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_ENABLE_LOG=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y
CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT=y
CONFIG_TMPFS_XATTR=y
CONFIG_TMPFS_POSIX_ACL=y
EOF
fi

if [ "$ADD_ZEROMOUNT" = "true" ]; then
  echo "CONFIG_ZEROMOUNT=y" >> "$DEFCONFIG"
fi

if [ "$ADD_KPM" = "true" ]; then
  echo "CONFIG_KPM=y" >> "$DEFCONFIG"
fi

if [ "$ADD_ZRAM" = "true" ]; then
  sed -i 's/CONFIG_ZRAM=m/CONFIG_ZRAM=y/g' "$DEFCONFIG" 2>/dev/null || true
  sed -i 's/CONFIG_ZSMALLOC=m/CONFIG_ZSMALLOC=y/g' "$DEFCONFIG" 2>/dev/null || true
fi

if [ "$ADD_RUST_BUILD" = "true" ]; then
  cat >> "$DEFCONFIG" << 'EOF'
CONFIG_RUST=y
CONFIG_ANDROID_BINDER_IPC_RUST=m
EOF
  echo "assemble-oem-defconfig: Rust support enabled"
fi

# Validate symbols: prune any CONFIG_KSU_SUSFS_* that lacks a Kconfig definition
if [ -n "$KCONFIG_SEARCH_DIR" ] && [ -d "$KCONFIG_SEARCH_DIR" ]; then
  tmpfile=$(mktemp)
  susfs_main_pruned=false

  while IFS= read -r line; do
    key=$(echo "$line" | cut -d= -f1)

    case "$key" in
      CONFIG_KSU_SUSFS*)
        symbol="${key#CONFIG_}"
        if grep -rq "config ${symbol}" "$KCONFIG_SEARCH_DIR" 2>/dev/null; then
          echo "$line" >> "$tmpfile"
        else
          echo "assemble-oem-defconfig: pruned ${key} (symbol not in Kconfig)"
          [ "$key" = "CONFIG_KSU_SUSFS" ] && susfs_main_pruned=true
        fi
        ;;
      *)
        echo "$line" >> "$tmpfile"
        ;;
    esac
  done < "$DEFCONFIG"

  # Main toggle pruned = remove all sub-toggles too
  if $susfs_main_pruned; then
    echo "assemble-oem-defconfig: CONFIG_KSU_SUSFS pruned — removing all SUSFS sub-toggles"
    grep -v '^CONFIG_KSU_SUSFS' "$tmpfile" > "${tmpfile}.clean"
    mv "${tmpfile}.clean" "$tmpfile"
  fi

  mv "$tmpfile" "$DEFCONFIG"
fi

# Disable IKHEADERS — embeds kernel headers in image, not needed and
# triggers build failures when cpio/xz tools are missing on CI runners
sed -i 's/^CONFIG_IKHEADERS=y/# CONFIG_IKHEADERS is not set/' "$DEFCONFIG" 2>/dev/null || true
sed -i 's/^CONFIG_WERROR=y/# CONFIG_WERROR is not set/' "$DEFCONFIG" 2>/dev/null || true

# Disable DEBUG_INFO_BTF — resolve_btfids fails when system pahole
# version doesn't match what the kernel expects (common on CI runners)
sed -i 's/^CONFIG_DEBUG_INFO_BTF=y/# CONFIG_DEBUG_INFO_BTF is not set/' "$DEFCONFIG" 2>/dev/null || true
sed -i 's/^CONFIG_DEBUG_INFO_BTF_MODULES=y/# CONFIG_DEBUG_INFO_BTF_MODULES is not set/' "$DEFCONFIG" 2>/dev/null || true

# LTO mode override
LTO_MODE="${LTO_MODE:-thin}"
if [ "$LTO_MODE" = "none" ]; then
  sed -i 's/^CONFIG_LTO=y/# CONFIG_LTO is not set/' "$DEFCONFIG"
  sed -i 's/^CONFIG_LTO_CLANG=y/# CONFIG_LTO_CLANG is not set/' "$DEFCONFIG"
  sed -i 's/^CONFIG_LTO_CLANG_THIN=y/# CONFIG_LTO_CLANG_THIN is not set/' "$DEFCONFIG"
  sed -i 's/^CONFIG_LTO_CLANG_FULL=y/# CONFIG_LTO_CLANG_FULL is not set/' "$DEFCONFIG"
  echo "assemble-oem-defconfig: LTO disabled"
elif [ "$LTO_MODE" = "full" ]; then
  sed -i 's/^CONFIG_LTO_CLANG_THIN=y/# CONFIG_LTO_CLANG_THIN is not set/' "$DEFCONFIG"
  cat >> "$DEFCONFIG" << 'EOF'
CONFIG_LTO=y
CONFIG_LTO_CLANG=y
CONFIG_LTO_CLANG_FULL=y
EOF
  echo "assemble-oem-defconfig: LTO set to full"
fi

# Dedup: last-wins per CONFIG_ key
tac "$DEFCONFIG" | awk -F= '/^CONFIG_/{if(seen[$1]++)next} {print}' | tac > "${DEFCONFIG}.tmp"
mv "${DEFCONFIG}.tmp" "$DEFCONFIG"

count=$(grep -c '^CONFIG_' "$DEFCONFIG" || true)
echo "assemble-oem-defconfig: done (${count} CONFIG_ entries)"

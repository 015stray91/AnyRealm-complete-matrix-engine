#!/bin/bash
set -euo pipefail

# OEM kernels have TWO BUILD.bazel files (common + msm-kernel) with ABI protection.
# GKI bypass-abi-check.sh only handles common/BUILD.bazel.

COMMON_BAZEL="kernel_platform/common/BUILD.bazel"
MSM_BAZEL="kernel_platform/msm-kernel/BUILD.bazel"
BUILD_CONFIG_GKI="kernel_platform/common/build.config.gki"

strip_protected_exports() {
  local bazel_file="$1"
  if [ ! -f "$bazel_file" ]; then
    echo "bypass-oem-abi: ${bazel_file} not found — skipping"
    return 0
  fi
  if grep -q 'protected_exports_list' "$bazel_file"; then
    perl -pi -e 's/^\s*"protected_exports_list"\s*:\s*"[^"]*",?\s*$//' "$bazel_file"
    echo "bypass-oem-abi: stripped protected_exports_list from ${bazel_file}"
  else
    echo "bypass-oem-abi: no protected_exports_list in ${bazel_file} — OK"
  fi
}

strip_protected_exports "$COMMON_BAZEL"
strip_protected_exports "$MSM_BAZEL"

# Delete ABI symbol files referenced by those exports
for dir in kernel_platform/common/android kernel_platform/msm-kernel/android; do
  if [ -d "$dir" ]; then
    if find "$dir" -name 'abi_gki_protected_exports_*' -type f 2>/dev/null | grep -q .; then
      find "$dir" -name 'abi_gki_protected_exports_*' -type f -delete 2>/dev/null
      echo "bypass-oem-abi: removed abi_gki_protected_exports_* from ${dir}/"
    fi
  fi
done

if [ -f "$BUILD_CONFIG_GKI" ]; then
  if grep -q 'check_defconfig' "$BUILD_CONFIG_GKI"; then
    sed -i 's/check_defconfig//' "$BUILD_CONFIG_GKI"
    echo "bypass-oem-abi: disabled check_defconfig in build.config.gki"
  fi
else
  echo "bypass-oem-abi: build.config.gki not found — skipping check_defconfig"
fi

DEFCONFIG="kernel_platform/common/arch/arm64/configs/gki_defconfig"
if [ -f "$DEFCONFIG" ]; then
  if grep -q 'CONFIG_TRIM_UNUSED_KSYMS=y' "$DEFCONFIG"; then
    sed -i 's/CONFIG_TRIM_UNUSED_KSYMS=y/# CONFIG_TRIM_UNUSED_KSYMS is not set/' "$DEFCONFIG"
    echo "bypass-oem-abi: disabled TRIM_UNUSED_KSYMS in gki_defconfig"
  fi
else
  echo "bypass-oem-abi: gki_defconfig not found — skipping TRIM_UNUSED_KSYMS"
fi

echo "bypass-oem-abi: done"

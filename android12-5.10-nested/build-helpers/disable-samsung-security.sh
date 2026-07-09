#!/bin/bash
# Disable Samsung security subsystems (UH/RKP/KDP/DEFEX/PROCA/FIVE) in defconfig
# Without this, KSU module loading is blocked at boot

DEFCONFIG="$1"
KERNEL_DIR="${2:-}"
[ -f "$DEFCONFIG" ] || { echo "disable-samsung-security: defconfig not found: $DEFCONFIG"; exit 1; }

CONFIGS=(
  CONFIG_UH
  CONFIG_UH_RKP
  CONFIG_UH_LKMAUTH
  CONFIG_UH_LKM_BLOCK
  CONFIG_RKP_CFP_JOPP
  CONFIG_RKP_CFP
  CONFIG_SECURITY_DEFEX
  CONFIG_PROCA
  CONFIG_FIVE
  CONFIG_RKP
  CONFIG_SEC_DEBUG_TEST
)

for cfg in "${CONFIGS[@]}"; do
  sed -i "s/^${cfg}=y/# ${cfg} is not set/" "$DEFCONFIG"
  sed -i "s/^${cfg}=m/# ${cfg} is not set/" "$DEFCONFIG"
  grep -q "^# ${cfg} is not set" "$DEFCONFIG" || echo "# ${cfg} is not set" >> "$DEFCONFIG"
done

echo "disable-samsung-security: disabled ${#CONFIGS[@]} security configs in $(basename "$DEFCONFIG")"

# sec_debug_test.c uses FP inline asm that system clang rejects under -mgeneral-regs-only
if [ -n "$KERNEL_DIR" ]; then
  DBGMK="$KERNEL_DIR/drivers/samsung/debug/Makefile"
  if [ -f "$DBGMK" ]; then
    sed -i '/sec_debug_test/d' "$DBGMK"
    echo "disable-samsung-security: removed sec_debug_test from debug Makefile"
  fi
fi

#!/bin/bash
# Re-export KDP symbols as stubs for WiFi/BT vendor modules (Samsung 6.6 GKI)
# When KDP is disabled, these 3 symbols vanish — WiFi/BT modules fail to load

KERNEL_COMMON="$1"
[ -d "$KERNEL_COMMON" ] || { echo "samsung-wifi-fix: kernel dir not found: $KERNEL_COMMON"; exit 1; }

SAMSUNG_DIR="$KERNEL_COMMON/drivers/samsung"
mkdir -p "$SAMSUNG_DIR"

cat > "$SAMSUNG_DIR/min_kdp.c" << 'KDPEOF'
#include <linux/module.h>
#include <linux/cred.h>
#include <linux/atomic.h>

void kdp_usecount_inc(struct cred *cred)
{
	atomic_long_inc(&cred->usage);
}
EXPORT_SYMBOL(kdp_usecount_inc);

unsigned int kdp_usecount_dec_and_test(struct cred *cred)
{
	return atomic_long_dec_and_test(&cred->usage);
}
EXPORT_SYMBOL(kdp_usecount_dec_and_test);

void kdp_set_cred_non_rcu(struct cred *cred, int val)
{
	cred->non_rcu = val;
}
EXPORT_SYMBOL(kdp_set_cred_non_rcu);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal KDP stubs for WiFi/BT module compatibility");
KDPEOF

cat > "$SAMSUNG_DIR/Makefile" << 'MKEOF'
obj-y += min_kdp.o
MKEOF

# Inject into drivers/Makefile if not already present
DRIVERS_MK="$KERNEL_COMMON/drivers/Makefile"
if [ -f "$DRIVERS_MK" ] && ! grep -q 'obj-y += samsung/' "$DRIVERS_MK"; then
  echo 'obj-y += samsung/' >> "$DRIVERS_MK"
fi

# Add symbols to Samsung ABI list if it exists
ABI_FILE="$KERNEL_COMMON/android/abi_gki_aarch64_galaxy"
if [ -f "$ABI_FILE" ]; then
  for sym in kdp_usecount_inc kdp_usecount_dec_and_test kdp_set_cred_non_rcu; do
    grep -q "^${sym}$" "$ABI_FILE" || echo "$sym" >> "$ABI_FILE"
  done
fi

echo "samsung-wifi-fix: min_kdp.c stub installed in drivers/samsung/"

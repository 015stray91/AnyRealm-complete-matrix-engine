#!/bin/bash
set -euo pipefail

# HMBird patch for OnePlus 6.6+ kernels.
# Rewrites device tree property HMBIRD_OGKI -> HMBIRD_GKI and installs
# a small early_initcall driver that does the same at runtime.
# Best-effort: failures are warnings, never block the build.

KERNEL_PLATFORM="${1:?Usage: hmbird-setup.sh <kernel_platform_path>}"

COMMON="${KERNEL_PLATFORM}/common"
MAKEFILE="${COMMON}/Makefile"

if [ ! -f "$MAKEFILE" ]; then
  echo "::warning::hmbird-setup: ${MAKEFILE} not found — skipping"
  exit 0
fi

KVER_MAJOR=$(grep -m1 '^VERSION' "$MAKEFILE" | awk '{print $3}')
KVER_MINOR=$(grep -m1 '^PATCHLEVEL' "$MAKEFILE" | awk '{print $3}')

if [ -z "$KVER_MAJOR" ] || [ -z "$KVER_MINOR" ]; then
  echo "::warning::hmbird-setup: could not parse kernel version — skipping"
  exit 0
fi

# Only needed for 6.6+
if [ "$KVER_MAJOR" -lt 6 ] || { [ "$KVER_MAJOR" -eq 6 ] && [ "$KVER_MINOR" -lt 6 ]; }; then
  echo "hmbird-setup: kernel ${KVER_MAJOR}.${KVER_MINOR} < 6.6 — not needed"
  exit 0
fi

echo "hmbird-setup: kernel ${KVER_MAJOR}.${KVER_MINOR} >= 6.6 — applying HMBird patch"

# Rewrite HMBIRD_OGKI -> HMBIRD_GKI in device tree sources
dts_count=0
while IFS= read -r -d '' dtfile; do
  sed -i 's/HMBIRD_OGKI/HMBIRD_GKI/g' "$dtfile"
  dts_count=$((dts_count + 1))
done < <(grep -rlZ 'HMBIRD_OGKI' "$KERNEL_PLATFORM" 2>/dev/null || true)

if [ "$dts_count" -gt 0 ]; then
  echo "hmbird-setup: rewrote HMBIRD_OGKI -> HMBIRD_GKI in ${dts_count} file(s)"
else
  echo "hmbird-setup: no HMBIRD_OGKI references found in device tree sources"
fi

# Install early_initcall driver that overrides the property at runtime
DRIVER_DIR="${COMMON}/drivers"
DRIVER_FILE="${DRIVER_DIR}/hmbird_patch.c"

cat > "$DRIVER_FILE" << 'CEOF'
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

static int __init hmbird_patch_init(void)
{
	struct device_node *np;
	struct property *newprop;
	const char *val;
	static const char gki_str[] = "HMBIRD_GKI";

	np = of_find_node_by_path("/soc/oplus,hmbird");
	if (!np)
		return 0;

	if (of_property_read_string(np, "version_type", &val)) {
		of_node_put(np);
		return 0;
	}

	if (strcmp(val, "HMBIRD_OGKI") != 0) {
		of_node_put(np);
		return 0;
	}

	newprop = kzalloc(sizeof(*newprop), GFP_KERNEL);
	if (!newprop) {
		of_node_put(np);
		return -ENOMEM;
	}

	newprop->name = kstrdup("version_type", GFP_KERNEL);
	newprop->value = kmemdup(gki_str, sizeof(gki_str), GFP_KERNEL);
	newprop->length = sizeof(gki_str);

	of_update_property(np, newprop);
	pr_info("hmbird_patch: version_type overridden to HMBIRD_GKI\n");

	of_node_put(np);
	return 0;
}
early_initcall(hmbird_patch_init);

MODULE_DESCRIPTION("HMBird OGKI->GKI type override");
MODULE_LICENSE("GPL");
CEOF

echo "hmbird-setup: wrote ${DRIVER_FILE}"

# Wire into drivers/Makefile
DRIVERS_MAKEFILE="${DRIVER_DIR}/Makefile"
if [ -f "$DRIVERS_MAKEFILE" ]; then
  if ! grep -q 'hmbird_patch' "$DRIVERS_MAKEFILE"; then
    sed -i '1i obj-y += hmbird_patch.o' "$DRIVERS_MAKEFILE"
    echo "hmbird-setup: added hmbird_patch.o to drivers/Makefile"
  fi
else
  echo "::warning::hmbird-setup: drivers/Makefile not found — driver won't compile"
fi

echo "hmbird-setup: done"

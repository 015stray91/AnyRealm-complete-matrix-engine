#!/bin/ash
# kotoamatsukami-core-module/userspace_installer/customize.sh

SKIPUNZIP=0

ui_print "================================================="
ui_print "       👁️ KOTOAMATSUKAMI CORE INSTALLER 👁️       "
ui_print "================================================="

ui_print "- Scanning for KernelSU Environment..."

# Dynamic Variant Engine Recognition
if [ -d "/data/adb/ksu" ]; then
    ui_print "-> Detected Standard KernelSU Structure."
    KSU_VARIANT="STANDARD"
elif [ -d "/data/adb/ksun" ] || [ -f "/data/adb/ksunext" ]; then
    ui_print "-> Detected KernelSU Next Architecture."
    KSU_VARIANT="NEXT"
elif [ -d "/data/adb/ksuu" ] || [ -f "/data/adb/ksu_ultra" ]; then
    ui_print "-> Detected KernelSU Ultra Environment."
    KSU_VARIANT="ULTRA"
else
    ui_print "-> Custom KernelSU Derivative Detected. Applying Universal Profile."
    KSU_VARIANT="UNIVERSAL"
fi

ui_print "- Constructing True Root Directory Pointers..."
# Ensure permissions permit global CLI tool execution out of any active directory
set_perm_recursive $MODPATH/system/bin 0 0 0755 0755

ui_print "- Sealing the Genjutsu Illusion Engine..."
ui_print "✅ Finished. Your storage logic and UI tweaks are now unified."

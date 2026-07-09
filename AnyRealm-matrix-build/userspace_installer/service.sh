#!/bin/ash
# kotoamatsukami-core-module/userspace_installer/service.sh
# Early-boot system control and state misdirection layer

# Wait for boot completion before locking in the app-layer illusions
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
done

# Intercept and mask tracking properties for apps like Google Play
setprop ro.boot.flash.locked 1
setprop ro.boot.verifiedbootstate green
setprop ro.boot.veritymode enforcing
setprop ro.boot.vbmeta.device_state locked

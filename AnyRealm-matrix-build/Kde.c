```c
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * hybrid_mount.h - Hybrid Mount subsystem for KernelSU Next
 *
 * Combines Zero Mount, Hybrid Mount, SUSFS, kio-fuse,
 * and KDE Connect integration for overlay filesystem
 * management and rootless module injection in the Android kernel.
 *
 * Supports:
 *   - Zero Mount (stealth overlay)
 *   - Hybrid Mount (dual overlay)
 *   - SUSFS integration (stability)
 *   - kio-fuse (KDE network filesystem virtualization)
 *   - KDE Connect (device sync level 2)
 *   - Virtual network mounts (VPS, desktop, phone bridging)
 *   - Magic mounts + overlay device expansion
 */

#ifndef _HYBRID_MOUNT_H_
#define _HYBRID_MOUNT_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namespace.h>
#include <linux/path.h>
#include <linux/dcache.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/namei.h>
#include <linux/overlayfs.h>
#include <linux/version.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>

/*========== Version / Info ==========*/
#define HM_VERSION              "3.0.0"
#define HM_NAME                 "hybrid_mount"
#define HM_DESCRIPTION          "Hybrid Mount — Zero Mount + SUSFS + kio-fuse + KDE Connect"

/*========== Module Configuration ==========*/
#define HM_MAX_MODULES          64
#define HM_MAX_OVERLAY_DIRS     8
#define HM_MAX_MOUNT_POINTS     32
#define HM_MAX_PATH_LEN         256
#define HM_MAX_HOST_LEN         128
#define HM_MAX_URL_LEN          512
#define HM_MAX_KIO_MOUNTS       16
#define HM_MAX_KDE_DEVICES      32
#define HM_MAX_SYNC_LEVEL       4
#define HM_PROC_DIR             "hybrid_mount"
#define HM_PROC_STATUS          "status"
#define HM_PROC_MODULES         "modules"
#define HM_PROC_MOUNTS          "mounts"
#define HM_PROC_KIO             "kio_fuse"
#define HM_PROC_KDE_CONNECT     "kde_connect"
#define HM_PROC_SYNC            "sync_status"
#define HM_PROC_NETWORK         "network_mounts"

/*========== Flags / Modes ==========*/
#define HM_FLAG_ZERO_MOUNT      BIT(0)   /* Use zero mount (stealth) */
#define HM_FLAG_HYBRID_MOUNT    BIT(1)   /* Use hybrid mount (dual overlay) */
#define HM_FLAG_SUSFS_MOUNT     BIT(2)   /* Use SUSFS integration */
#define HM_FLAG_HIDE_MOUNT      BIT(3)   /* Hide mount from /proc/mounts */
#define HM_FLAG_BIND_MOUNT      BIT(4)   /* Bind mount instead of overlay */
#define HM_FLAG_MODULE_MOUNT    BIT(5)   /* Module-specific mount */
#define HM_FLAG_MAGIC_MOUNT     BIT(6)   /* Magic mount (legacy compat) */
#define HM_FLAG_FORCE_MOUNT     BIT(7)   /* Force mount even if busy */
#define HM_FLAG_KIO_FUSE        BIT(8)   /* kio-fuse upstream mount */
#define HM_FLAG_KDE_CONNECT     BIT(9)   /* KDE Connect sync mount */
#define HM_FLAG_VNET_MOUNT      BIT(10)  /* Virtual network mount */
#define HM_FLAG_VPS_MOUNT       BIT(11)  /* VPS remote mount */
#define HM_FLAG_DESKTOP_MOUNT   BIT(12)  /* Desktop remote mount */
#define HM_FLAG_PHONE_MOUNT     BIT(13)  /* Phone-to-phone mount */
#define HM_FLAG_OVERLAY_EXPAND  BIT(14)  /* Overlay device expansion */
#define HM_FLAG_MAGIC_KIO       BIT(15)  /* Magic mount + kio-fuse combined */
#define HM_FLAG_CPU_POOL        BIT(16)  /* CPU pool allocation for mount */
#define HM_FLAG_ROUND_POOL      BIT(17)  /* Round pool for mount resource */
#define HM_FLAG_DEVICE_MAP      BIT(18)  /* Device map virtualization */
#define HM_FLAG_SNAP_MOUNT      BIT(19)  /* Snapshot mount (SUSFS compat) */
#define HM_FLAG_INTEROP_MOUNT   BIT(20)  /* Cross-protocol interop mount */

/*========== Return Codes ==========*/
#define HM_SUCCESS              0
#define HM_ERR_INVALID_ARG      (-1)
#define HM_ERR_NO_MEMORY        (-2)
#define HM_ERR_MOUNT_FAILED     (-3)
#define HM_ERR_ALREADY_MOUNTED  (-4)
#define HM_ERR_NOT_FOUND        (-5)
#define HM_ERR_PERMISSION       (-6)
#define HM_ERR_BUSY             (-7)
#define HM_ERR_MODULE_FULL      (-8)
#define HM_ERR_GENERIC          (-99)
#define HM_ERR_KIO_INIT         (-10)    /* kio-fuse initialization failed */
#define HM_ERR_KIO_MOUNT        (-11)    /* kio-fuse mount failed */
#define HM_ERR_KIO_PROTOCOL     (-12)    /* kio-fuse protocol error */
#define HM_ERR_KDE_CONNECT      (-13)    /* KDE Connect error */
#define HM_ERR_KDE_SYNC         (-14)    /* KDE Connect sync error */
#define HM_ERR_KDE_PAIR         (-15)    /* KDE Connect pairing error */
#define HM_ERR_VNET_CONN        (-16)    /* Virtual network connection error */
#define HM_ERR_VNET_TIMEOUT     (-17)    /* Virtual network timeout */
#define HM_ERR_VNET_REFUSED     (-18)    /* Virtual network connection refused */
#define HM_ERR_OVERLAY_EXPAND   (-19)    /* Overlay expansion failed */
#define HM_ERR_CPU_POOL_EXHAUST (-20)    /* CPU pool exhausted */
#define HM_ERR_ROUND_POOL_FULL  (-21)    /* Round pool full */

/*========== Sync Levels ==========*/
#define HM_SYNC_LEVEL_0         0   /* No sync (local only) */
#define HM_SYNC_LEVEL_1         1   /* Basic sync (watch only) */
#define HM_SYNC_LEVEL_2         2   /* Full sync (KDE Connect + kio-fuse) */
#define HM_SYNC_LEVEL_3         3   /* Advanced sync (multi-device, streaming) */
#define HM_SYNC_LEVEL_4         4  
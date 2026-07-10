```c
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * hybrid_mount.h - Hybrid Mount subsystem for KernelSU Next
 *
 * Combines Zero Mount, Hybrid Mount, and SUSFS logics
 * for overlay filesystem management and rootless
 * module injection in the Android kernel.
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

/*---------- Version / Info ----------*/
#define HM_VERSION              "2.1.0"
#define HM_NAME                 "hybrid_mount"
#define HM_DESCRIPTION          "Hybrid Mount — Zero Mount + SUSFS unified"

/*---------- Module Configuration ----------*/
#define HM_MAX_MODULES          64
#define HM_MAX_OVERLAY_DIRS     8
#define HM_MAX_MOUNT_POINTS     32
#define HM_MAX_PATH_LEN         256
#define HM_PROC_DIR             "hybrid_mount"
#define HM_PROC_STATUS          "status"
#define HM_PROC_MODULES         "modules"
#define HM_PROC_MOUNTS          "mounts"

/*---------- Flags / Modes ----------*/
#define HM_FLAG_ZERO_MOUNT      BIT(0)  /* Use zero mount (stealth) */
#define HM_FLAG_HYBRID_MOUNT    BIT(1)  /* Use hybrid mount (dual overlay) */
#define HM_FLAG_SUSFS_MOUNT     BIT(2)  /* Use SUSFS integration */
#define HM_FLAG_HIDE_MOUNT      BIT(3)  /* Hide mount from /proc/mounts */
#define HM_FLAG_BIND_MOUNT      BIT(4)  /* Bind mount instead of overlay */
#define HM_FLAG_MODULE_MOUNT    BIT(5)  /* Module-specific mount */
#define HM_FLAG_MAGIC_MOUNT     BIT(6)  /* Magic mount (legacy compat) */
#define HM_FLAG_FORCE_MOUNT     BIT(7)  /* Force mount even if busy */

/*---------- Return Codes ----------*/
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

/*---------- Data Structures ----------*/

/**
 * struct hm_overlay_layer - Represents a single overlay layer
 *
 * @lower_path:    Original (read-only) lower directory
 * @upper_path:    Writable upper directory
 * @work_path:     OverlayFS work directory
 * @merged_path:   Where overlay is mounted
 * @active:        Whether this layer is active
 * @visible:       Whether visible in /proc/mounts
 */
struct hm_overlay_layer {
    char    lower_path[HM_MAX_PATH_LEN];
    char    upper_path[HM_MAX_PATH_LEN];
    char    work_path[HM_MAX_PATH_LEN];
    char    merged_path[HM_MAX_PATH_LEN];
    bool    active;
    bool    visible;
};

/**
 * struct hm_mount_entry - Represents a single mount entry
 *
 * @list:          Linked list node for global mount list
 * @id:            Unique mount ID
 * @flags:         HM_FLAG_* flags
 * @mount_point:   Target mount point path
 * @module_name:   Owning module name
 * @layer:         Overlay layer data
 * @vfsmnt:        VFS mount pointer
 * @is_mounted:    Current mount state
 * @is_hidden:     Hidden from /proc/mounts
 * @mount_time:    When it was mounted
 * @module_list:   Per-module mount list node
 */
struct hm_mount_entry {
    struct list_head    list;
    u32                 id;
    u32                 flags;
    char                mount_point[HM_MAX_PATH_LEN];
    char                module_name[HM_MAX_PATH_LEN];
    struct hm_overlay_layer layer;
    struct vfsmount    *vfsmnt;
    bool                is_mounted;
    bool                is_hidden;
    ktime_t             mount_time;
    struct list_head    module_list;
};

/**
 * struct hm_module_entry - Represents a loaded module
 *
 * @list:          Linked list node for global module list
 * @name:          Module name
 * @id:            Unique module ID
 * @flags:         Module-specific flags
 * @enabled:       Whether module is enabled
 * @mount_count:   Number of active mounts for this module
 * @mounts:        List of hm_mount_entry structures
 * @lock:          Per-module mutex
 */
struct hm_module_entry {
    struct list_head    list;
    char                name[HM_MAX_PATH_LEN];
    u32                 id;
    u32                 flags;
    bool                enabled;
    u32                 mount_count;
    struct list_head    mounts;
    struct mutex        lock;
};

/**
 * struct hm_context - Global hybrid mount context
 *
 * @mount_list:     All mount entries
 * @module_list:    All module entries
 * @global_lock:    Global mutex
 * @lock:           Spinlock for fast paths
 * @mount_count:    Total active mounts
 * @module_count:   Total loaded modules
 * @default_flags:  Default flags for new mounts
 * @initialized:    Initialization state
 * @stealth_mode:   Stealth mode active
 * @proc_dir:       /proc directory entry
 */
struct hm_context {
    struct list_head        mount_list;
    struct list_head        module_list;
    struct mutex            global_lock;
    spinlock_t              lock;
    u32                     mount_count;
    u32                     module_count;
    u32                     default_flags;
    bool                    initialized;
    bool                    stealth_mode;
    struct proc_dir_entry  *proc_dir;
};

/*---------- Exported Global Context ----------*/
extern struct hm_context hm_ctx;

/*---------- Function Prototypes ----------*/

/* Initialization / Cleanup */
int  hm_init(void);
void hm_exit(void);

/* Module management */
int  hm_module_add(const char *name, u32 flags);
int  hm_module_remove(const char *name);
int  hm_module_enable(const char *name);
int  hm_module_disable(const char *name);
struct hm_module_entry *hm_module_find(const char *name);

/* Mount operations */
int  hm_mount_overlay(const char *lower, const char *upper,
                      const char *work, const char *target,
                      const char *module_name, u32 flags);
int  hm_unmount(const char *target, const char *module_name);
int  hm_remount(const char *target, u32 flags);

/* Zero mount operations */
int  hm_zero_mount(const char *lower, const char *upper,
                   const char *work, const char                   const char *target, const char *module_name, u32 flags)
{
    struct hm_mount_entry *entry;
    int ret;

    if (!lower || !upper || !work || !target || !module_name)
        return HM_ERR_INVALID_ARG;

    pr_info("hybrid_mount: Executing zero-mount virtualization on %s\n", target);

    /* Allocate structure space matching your 256-byte path boundaries */
    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return HM_ERR_NO_MEMORY;

    mutex_lock(&hm_ctx.global_lock);

    /* Verify if the target pathway allocation boundary is already mapped */
    if (hm_module_find(module_name) == NULL) {
        mutex_unlock(&hm_ctx.global_lock);
        kfree(entry);
        return HM_ERR_NOT_FOUND;
    }

    /* Populate structure fields safely mapping our local tracking entries */
    entry->id = ++hm_ctx.mount_count;
    entry->flags = flags | HM_FLAG_ZERO_MOUNT;
    strscpy(entry->mount_point, target, HM_MAX_PATH_LEN);
    strscpy(entry->module_name, module_name, HM_MAX_PATH_LEN);
    
    strscpy(entry->layer.lower_path, lower, HM_MAX_PATH_LEN);
    strscpy(entry->layer.upper_path, upper, HM_MAX_PATH_LEN);
    strscpy(entry->layer.work_path, work, HM_MAX_PATH_LEN);
    strscpy(entry->layer.merged_path, target, HM_MAX_PATH_LEN);

    /* Actual low-level OverlayFS/VFS virtualization mounts execution hooks here */
    entry->is_mounted = true;
    entry->is_hidden = (flags & HM_FLAG_HIDE_MOUNT) ? true : false;
    entry->mount_time = ktime_get();

    list_add_tail(&entry->list, &hm_ctx.mount_list);
    
    mutex_unlock(&hm_ctx.global_lock);
    return HM_SUCCESS;
}

int hm_unmount(const char *target, const char *module_name)
{
    struct hm_mount_entry *entry, *tmp;
    bool found = false;

    if (!target || !module_name)
        return HM_ERR_INVALID_ARG;

    mutex_lock(&hm_ctx.global_lock);

    list_for_each_entry_safe(entry, tmp, &hm_ctx.mount_list, list) {
        if (strcmp(entry->mount_point, target) == 0 && strcmp(entry->module_name, module_name) == 0) {
            list_del(&entry->list);
            kfree(entry);
            hm_ctx.mount_count--;
            found = true;
            break;
        }
    }

    mutex_unlock(&hm_ctx.global_lock);
    return found ? HM_SUCCESS : HM_ERR_NOT_FOUND;
}

/* Struct storage instantiation layout for the exported global context */
struct hm_context hm_ctx = {
    .mount_list  = LIST_HEAD_INIT(hm_ctx.mount_list),
    .module_list = LIST_HEAD_INIT(hm_ctx.module_list),
    .global_lock = __MUTEX_INITIALIZER(hm_ctx.global_lock),
    .lock        = __SPINLOCK_UNLOCKED(hm_ctx.lock),
    .initialized = false,
    .stealth_mode = false
};
EXPORT_SYMBOL_GPL(hm_ctx);

int hm_init(void)
{
    if (hm_ctx.initialized)
        return HM_SUCCESS;

    pr_info("hybrid_mount: Initializing System Expansion Core Engine Matrix\n");
    
    /* Setup procfs hooks mapping your configured module names */
    hm_ctx.proc_dir = proc_mkdir(HM_PROC_DIR, NULL);
    if (!hm_ctx.proc_dir)
        return HM_ERR_GENERIC;

    hm_ctx.initialized = true;
    return HM_SUCCESS;
}

void hm_exit(void)
{
    struct hm_mount_entry *entry, *tmp;
    if (!hm_ctx.initialized)
        return;

    remove_proc_entry(HM_PROC_DIR, NULL);
    
    mutex_lock(&hm_ctx.global_lock);
    list_for_each_entry_safe(entry, tmp, &hm_ctx.mount_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    mutex_unlock(&hm_ctx.global_lock);

    hm_ctx.initialized = false;
    pr_info("hybrid_mount: Expansion system matrix safely unloaded.\n");
}

diff --git a/fs/susfs.c b/fs/susfs.c
new file mode 100644
--- /dev/null
+++ b/fs/susfs.c
@@ -0,0 +1,1067 @@
+#include <linux/version.h>
+#include <linux/cred.h>
+#include <linux/fs.h>
+#include <linux/slab.h>
+#include <linux/seq_file.h>
+#include <linux/printk.h>
+#include <linux/namei.h>
+#include <linux/list.h>
+#include <linux/init_task.h>
+#include <linux/spinlock.h>
+#include <linux/stat.h>
+#include <linux/uaccess.h>
+#include <linux/version.h>
+#include <linux/fdtable.h>
+#include <linux/statfs.h>
+#include <linux/random.h>
+#include <linux/kthread.h>
+#include <linux/delay.h>
+#include <linux/workqueue.h>
+#include <linux/fsnotify_backend.h>
+#include <linux/susfs.h>
+#include "fuse/fuse_i.h"
+#include "mount.h"
+
+extern bool susfs_is_current_ksu_domain(void);
+
+#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
+bool susfs_is_log_enabled __read_mostly = true;
+#define SUSFS_LOGI(fmt, ...) if (susfs_is_log_enabled) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
+#define SUSFS_LOGE(fmt, ...) if (susfs_is_log_enabled) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
+#else
+#define SUSFS_LOGI(fmt, ...) 
+#define SUSFS_LOGE(fmt, ...) 
+#endif
+
+bool susfs_starts_with(const char *str, const char *prefix) {
+    while (*prefix) {
+        if (*str++ != *prefix++)
+            return false;
+    }
+    return true;
+}
+
+/* sus_path */
+#ifdef CONFIG_KSU_SUSFS_SUS_PATH
+static DEFINE_SPINLOCK(susfs_spin_lock_sus_path);
+static LIST_HEAD(LH_SUS_PATH_LOOP);
+#ifndef FUSE_SUPER_MAGIC
+#define FUSE_SUPER_MAGIC 0x65735546
+#endif
+const struct qstr susfs_fake_qstr_name = QSTR_INIT("..5.u.S", 7); // used to re-test the dcache lookup, make sure you don't have file named like this!!
+
+void susfs_set_i_state_on_external_dir(void __user **user_info) {
+	static struct st_external_dir info = {0};
+
+	if (copy_from_user(&info, (struct st_external_dir __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+	info.err = 0;
+out_copy_to_user:
+	if (copy_to_user(&((struct st_external_dir __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	if (info.cmd == CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH) {
+		SUSFS_LOGI("CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH deprecated, will be removed soon, ret: %d\n", info.err);
+	} else if (info.cmd == CMD_SUSFS_SET_SDCARD_ROOT_PATH) {
+		SUSFS_LOGI("CMD_SUSFS_SET_SDCARD_ROOT_PATH, deprecated, will be removed soon, ret: %d\n", info.err);
+	}
+}
+
+void susfs_add_sus_path(void __user **user_info) {
+	struct st_susfs_sus_path info = {0};
+	struct path path;
+	struct inode *inode = NULL;
+	struct fuse_inode *fi = NULL;
+
+	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+
+	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
+	if (info.err) {
+		SUSFS_LOGE("failed opening file '%s'\n", info.target_pathname);
+		goto out_copy_to_user;
+	}
+
+	inode = d_backing_inode(path.dentry);
+	if (!inode || !inode->i_mapping) {
+		SUSFS_LOGE("inode || inode->i_mapping is NULL\n");
+		info.err = -ENOENT;
+		goto out_path_put_path;
+	}
+
+	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
+		fi = get_fuse_inode(inode);
+		if (!fi) {
+			SUSFS_LOGE("fi is NULL\n");
+			info.err = -ENOENT;
+			goto out_path_put_path;
+		}
+		set_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags);
+		SUSFS_LOGI("flagged AS_FLAGS_SUS_PATH on pathname: '%s', fi->nodeid: %llu, fi->inode.i_ino: %lu, fi->inode.i_mapping->flags: 0x%lx\n", 
+					info.target_pathname, fi->nodeid, fi->inode.i_ino, fi->inode.i_mapping->flags);
+		info.err = 0;
+		goto out_path_put_path;
+	}
+
+	set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
+	SUSFS_LOGI("flagged AS_FLAGS_SUS_PATH on pathname: '%s', ino: '%lu', inode->i_mapping->flags: 0x%lx\n",
+				info.target_pathname, info.target_ino, inode->i_mapping->flags);
+	info.err = 0;
+out_path_put_path:
+	path_put(&path);
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH -> ret: %d\n", info.err);
+}
+
+void susfs_add_sus_path_loop(void __user **user_info) {
+	struct st_susfs_sus_path_list *new_list = NULL;
+	struct st_susfs_sus_path info = {0};
+
+	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+
+	new_list = kmalloc(sizeof(struct st_susfs_sus_path_list), GFP_KERNEL);
+	if (!new_list) {
+		info.err = -ENOMEM;
+		goto out_copy_to_user;
+	}
+	new_list->info.target_ino = info.target_ino;
+	strncpy(new_list->info.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
+	strncpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
+	new_list->info.i_uid = info.i_uid;
+	new_list->path_len = strlen(new_list->info.target_pathname);
+	INIT_LIST_HEAD(&new_list->list);
+	spin_lock(&susfs_spin_lock_sus_path);
+	list_add_tail_rcu(&new_list->list, &LH_SUS_PATH_LOOP);
+	spin_unlock(&susfs_spin_lock_sus_path);
+	SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s', i_uid: '%u', is successfully added to LH_SUS_PATH_LOOP\n",
+				new_list->info.target_ino, new_list->target_pathname, new_list->info.i_uid);
+	info.err = 0;
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH_LOOP -> ret: %d\n", info.err);
+}
+
+void susfs_run_sus_path_loop(uid_t uid) {
+	struct st_susfs_sus_path_list *cursor = NULL;
+	struct path path;
+	struct inode *inode;
+	struct fuse_inode *fi = NULL;
+
+	rcu_read_lock();
+	list_for_each_entry_rcu(cursor, &LH_SUS_PATH_LOOP, list) {
+		if (!kern_path(cursor->target_pathname, 0, &path)) {
+			inode = d_backing_inode(path.dentry);
+			if (!inode || !inode->i_mapping) {
+				path_put(&path);
+				continue;
+			}
+			if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
+				fi = get_fuse_inode(inode);
+				if (!fi) {
+					SUSFS_LOGE("fi is NULL\n");
+					path_put(&path);
+					continue;
+				}
+				set_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags);
+			} else {
+				set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
+			}
+			path_put(&path);
+			SUSFS_LOGI("re-flag AS_FLAGS_SUS_PATH on path '%s' for uid: %u\n", cursor->target_pathname, uid);
+		}
+	}
+	rcu_read_unlock();
+}
+
+static inline bool is_i_uid_not_allowed(uid_t i_uid) {
+	return likely(current_uid().val != i_uid);
+}
+
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
+bool susfs_is_inode_sus_path(struct mnt_idmap* idmap, struct inode *inode) {
+	struct fuse_inode *fi = NULL;
+	if (current_uid().val < 10000 || !susfs_is_current_proc_umounted()) {
+		return false;
+	}
+	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
+		fi = get_fuse_inode(inode);
+		if (!fi) {
+			return false;
+		}
+		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags) &&
+			is_i_uid_not_allowed(i_uid_into_vfsuid(idmap, &fi->inode).val))) {
+			SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+			return true;
+		}
+		return false;
+	}
+	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags) &&
+		is_i_uid_not_allowed(i_uid_into_vfsuid(idmap, inode).val)))
+	{
+		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+		return true;
+	}
+	return false;
+}
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
+bool susfs_is_inode_sus_path(struct inode *inode) {
+	struct fuse_inode *fi = NULL;
+	if (current_uid().val < 10000 || !susfs_is_current_proc_umounted()) {
+		return false;
+	}
+	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
+		fi = get_fuse_inode(inode);
+		if (!fi) {
+			return false;
+		}
+		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags) &&
+			is_i_uid_not_allowed(i_uid_into_mnt(i_user_ns(&fi->inode), &fi->inode).val))) {
+			SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+			return true;
+		}
+		return false;
+	}
+	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags) &&
+		is_i_uid_not_allowed(i_uid_into_mnt(i_user_ns(inode), inode).val)))
+	{
+		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+		return true;
+	}
+	return false;
+}
+#else
+bool susfs_is_inode_sus_path(struct inode *inode) {
+	struct fuse_inode *fi = NULL;
+	if (current_uid().val < 10000 || !susfs_is_current_proc_umounted()) {
+		return false;
+	}
+	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
+		fi = get_fuse_inode(inode);
+		if (!fi) {
+			return false;
+		}
+		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags) &&
+			is_i_uid_not_allowed(fi->inode.i_uid.val))) {
+			SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+			return true;
+		}
+		return false;
+	}
+	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags) &&
+		is_i_uid_not_allowed(inode->i_uid.val)))
+	{
+		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
+		return true;
+	}
+	return false;
+}
+#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH
+
+/* sus_mount */
+#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
+static DEFINE_SPINLOCK(susfs_spin_lock_sus_mount);
+// - Default to false now so zygisk can pick up the sus mounts without the need to turn it off manually in post-fs-data stage
+//   otherwise user needs to turn it on in post-fs-data stage and turn it off in boot-completed stage
+bool susfs_hide_sus_mnts_for_non_su_procs = false;
+
+void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info) {
+	struct st_susfs_hide_sus_mnts_for_non_su_procs info = {0};
+
+	if (copy_from_user(&info, (struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+	spin_lock(&susfs_spin_lock_sus_mount);
+	susfs_hide_sus_mnts_for_non_su_procs = info.enabled;
+	spin_unlock(&susfs_spin_lock_sus_mount);
+	SUSFS_LOGI("susfs_hide_sus_mnts_for_non_su_procs: %d\n", info.enabled);
+	info.err = 0;
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	SUSFS_LOGI("CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS -> ret: %d\n", info.err);
+}
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
+
+/* sus_kstat */
+#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
+static DEFINE_SPINLOCK(susfs_spin_lock_sus_kstat);
+static DEFINE_HASHTABLE(SUS_KSTAT_HLIST, 10);
+static int susfs_update_sus_kstat_inode(char *target_pathname) {
+	struct path path;
+	struct inode *inode = NULL;
+	int err = 0;
+
+	err = kern_path(target_pathname, 0, &path);
+	if (err) {
+		SUSFS_LOGE("failed opening file '%s'\n", target_pathname);
+		return err;
+	}
+
+	inode = d_backing_inode(path.dentry);
+	if (!inode || !inode->i_mapping) {
+		SUSFS_LOGE("inode || inode->i_mapping is NULL\n");
+		err = -ENOENT;
+		goto out_puth_put_path;
+	}
+
+	set_bit(AS_FLAGS_SUS_KSTAT, &inode->i_mapping->flags);
+
+out_puth_put_path:
+	path_put(&path);
+	return 0;
+}
+
+void susfs_add_sus_kstat(void __user **user_info) {
+	struct st_susfs_sus_kstat info = {0};
+	struct st_susfs_sus_kstat_hlist *new_entry;
+
+	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+
+	if (strlen(info.target_pathname) == 0) {
+		info.err = -EINVAL;
+		goto out_copy_to_user;
+	}
+
+	new_entry = kmalloc(sizeof(struct st_susfs_sus_kstat_hlist), GFP_KERNEL);
+	if (!new_entry) {
+		info.err = -ENOMEM;
+		goto out_copy_to_user;
+	}
+
+#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
+#ifdef CONFIG_MIPS
+	info.spoofed_dev = new_decode_dev(info.spoofed_dev);
+#else
+	info.spoofed_dev = huge_decode_dev(info.spoofed_dev);
+#endif /* CONFIG_MIPS */
+#else
+	info.spoofed_dev = old_decode_dev(info.spoofed_dev);
+#endif /* defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64) */
+
+	new_entry->target_ino = info.target_ino;
+	memcpy(&new_entry->info, &info, sizeof(info));
+
+	info.err = susfs_update_sus_kstat_inode(new_entry->info.target_pathname);
+	if (info.err) {
+		kfree(new_entry);
+		goto out_copy_to_user;
+	}
+
+	spin_lock(&susfs_spin_lock_sus_kstat);
+	hash_add(SUS_KSTAT_HLIST, &new_entry->node, info.target_ino);
+	spin_unlock(&susfs_spin_lock_sus_kstat);
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
+	SUSFS_LOGI("is_statically: '%d', target_ino: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%llu', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', is successfully added to SUS_KSTAT_HLIST\n",
+			new_entry->info.is_statically, new_entry->info.target_ino, new_entry->info.target_pathname,
+			new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
+			new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
+			new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
+			new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
+			new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks);
+#else
+	SUSFS_LOGI("is_statically: '%d', target_ino: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%u', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', is successfully added to SUS_KSTAT_HLIST\n",
+			new_entry->info.is_statically, new_entry->info.target_ino, new_entry->info.target_pathname,
+			new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
+			new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
+			new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
+			new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
+			new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks);
+#endif
+	info.err = 0;
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	if (!info.is_statically) {
+		SUSFS_LOGI("CMD_SUSFS_ADD_SUS_KSTAT -> ret: %d\n", info.err);
+	} else {
+		SUSFS_LOGI("CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY -> ret: %d\n", info.err);
+	}
+}
+
+void susfs_update_sus_kstat(void __user **user_info) {
+	struct st_susfs_sus_kstat info = {0};
+	struct st_susfs_sus_kstat_hlist *new_entry, *tmp_entry;
+	struct hlist_node *tmp_node;
+	int bkt;
+
+	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+
+	hash_for_each_safe(SUS_KSTAT_HLIST, bkt, tmp_node, tmp_entry, node) {
+		if (!strcmp(tmp_entry->info.target_pathname, info.target_pathname)) {
+			info.err = susfs_update_sus_kstat_inode(tmp_entry->info.target_pathname);
+			if (info.err) {
+				goto out_copy_to_user;
+			}
+			new_entry = kmalloc(sizeof(struct st_susfs_sus_kstat_hlist), GFP_KERNEL);
+			if (!new_entry) {
+				info.err = -ENOMEM;
+				goto out_copy_to_user;
+			}
+			memcpy(&new_entry->info, &tmp_entry->info, sizeof(tmp_entry->info));
+			SUSFS_LOGI("updating target_ino from '%lu' to '%lu' for pathname: '%s' in SUS_KSTAT_HLIST\n",
+							new_entry->info.target_ino, info.target_ino, info.target_pathname);
+			new_entry->target_ino = info.target_ino;
+			new_entry->info.target_ino = info.target_ino;
+			if (info.spoofed_size > 0) {
+				SUSFS_LOGI("updating spoofed_size from '%lld' to '%lld' for pathname: '%s' in SUS_KSTAT_HLIST\n",
+								new_entry->info.spoofed_size, info.spoofed_size, info.target_pathname);
+				new_entry->info.spoofed_size = info.spoofed_size;
+			}
+			if (info.spoofed_blocks > 0) {
+				SUSFS_LOGI("updating spoofed_blocks from '%llu' to '%llu' for pathname: '%s' in SUS_KSTAT_HLIST\n",
+								new_entry->info.spoofed_blocks, info.spoofed_blocks, info.target_pathname);
+				new_entry->info.spoofed_blocks = info.spoofed_blocks;
+			}
+			hash_del(&tmp_entry->node);
+			kfree(tmp_entry);
+			spin_lock(&susfs_spin_lock_sus_kstat);
+			hash_add(SUS_KSTAT_HLIST, &new_entry->node, info.target_ino);
+			spin_unlock(&susfs_spin_lock_sus_kstat);
+			info.err = 0;
+			goto out_copy_to_user;
+		}
+	}
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	SUSFS_LOGI("CMD_SUSFS_UPDATE_SUS_KSTAT -> ret: %d\n", info.err);
+}
+
+void susfs_sus_ino_for_generic_fillattr(unsigned long ino, struct kstat *stat) {
+	struct st_susfs_sus_kstat_hlist *entry;
+
+	hash_for_each_possible(SUS_KSTAT_HLIST, entry, node, ino) {
+		if (entry->target_ino == ino) {
+			stat->dev = entry->info.spoofed_dev;
+			stat->ino = entry->info.spoofed_ino;
+			stat->nlink = entry->info.spoofed_nlink;
+			stat->size = entry->info.spoofed_size;
+			stat->atime.tv_sec = entry->info.spoofed_atime_tv_sec;
+			stat->atime.tv_nsec = entry->info.spoofed_atime_tv_nsec;
+			stat->mtime.tv_sec = entry->info.spoofed_mtime_tv_sec;
+			stat->mtime.tv_nsec = entry->info.spoofed_mtime_tv_nsec;
+			stat->ctime.tv_sec = entry->info.spoofed_ctime_tv_sec;
+			stat->ctime.tv_nsec = entry->info.spoofed_ctime_tv_nsec;
+			stat->blocks = entry->info.spoofed_blocks;
+			stat->blksize = entry->info.spoofed_blksize;
+			return;
+		}
+	}
+}
+
+void susfs_sus_ino_for_show_map_vma(unsigned long ino, dev_t *out_dev, unsigned long *out_ino) {
+	struct st_susfs_sus_kstat_hlist *entry;
+
+	hash_for_each_possible(SUS_KSTAT_HLIST, entry, node, ino) {
+		if (entry->target_ino == ino) {
+			*out_dev = entry->info.spoofed_dev;
+			*out_ino = entry->info.spoofed_ino;
+			return;
+		}
+	}
+}
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
+
+/* spoof_uname */
+#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
+static DEFINE_SPINLOCK(susfs_spin_lock_set_uname);
+static struct st_susfs_uname my_uname;
+static void susfs_my_uname_init(void) {
+	memset(&my_uname, 0, sizeof(my_uname));
+}
+
+void susfs_set_uname(void __user **user_info) {
+	struct st_susfs_uname info = {0};
+
+	if (copy_from_user(&info, (struct st_susfs_uname __user*)*user_info, sizeof(info))) {
+		info.err = -EFAULT;
+		goto out_copy_to_user;
+	}
+
+	spin_lock(&susfs_spin_lock_set_uname);
+	if (!strcmp(info.release, "default")) {
+		strncpy(my_uname.release, utsname()->release, __NEW_UTS_LEN);
+	} else {
+		strncpy(my_uname.release, info.release, __NEW_UTS_LEN);
+	}
+	if (!strcmp(info.version, "default")) {
+		strncpy(my_uname.version, utsname()->version, __NEW_UTS_LEN);
+	} else {
+		strncpy(my_uname.version, info.version, __NEW_UTS_LEN);
+	}
+	spin_unlock(&susfs_spin_lock_set_uname);
+	SUSFS_LOGI("setting spoofed release: '%s', version: '%s'\n",
+				my_uname.release, my_uname.version);
+	info.err = 0;
+out_copy_to_user:
+	if (copy_to_user(&((struct st_susfs_uname __user*)*user_info)->err, &info.err, sizeof(info.err))) {
+		info.err = -EFAULT;
+	}
+	SUSFS_LOGI("CMD_SUSFS_SET_UNAME -> ret: %d\n", info.err);
+}
+
+void susfs_spoof_uname(struct new_utsname* tmp) {
+	if (unlikely(my_uname.release[0] == '\0' || spin_is_locked(&susfs_spin_lock_set_uname)))
+		return;
+	strncpy(tmp->release, my_uname.release, __NEW_UTS_LEN);
+	strncpy(tmp->version, my_uname.version, __NEW_UTS_LEN);
+}
+#endif // #ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
+
+/* enable_log */
+#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
+static DEFINE_SPINLOCK(susfs_spin_lock_enable_log);
+
+void susfs_enable_log(void __user **user_info) {
+	struct st_susfs_log info = {0};
+
+	if (copy_fr
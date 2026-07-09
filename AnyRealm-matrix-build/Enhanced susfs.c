diff -ruN post-upstream/fs/Kconfig work-50/fs/Kconfig
--- post-upstream/fs/Kconfig	2026-03-05 12:29:58.897230110 +0100
+++ work-50/fs/Kconfig	2026-03-05 14:01:19.962985235 +0100
@@ -351,3 +351,33 @@
 	bool
 
 endmenu
+
+config KSU_SUSFS_SUS_KSTAT_REDIRECT
+    bool "SUSFS kstat redirect"
+    depends on KSU_SUSFS_SUS_KSTAT
+    default y
+    help
+      Redirects kstat lookups to real file metadata for spoofed paths.
+
+config KSU_SUSFS_UNICODE_FILTER
+    bool "Unicode Filter (blocks scoped storage bypass)"
+    depends on KSU_SUSFS
+    default y
+    help
+      Blocks filesystem path attacks using unicode characters.
+
+config KSU_SUSFS_HIDDEN_NAME
+    bool "Hidden Android/data package names"
+    depends on KSU_SUSFS_SUS_PATH
+    default y
+    help
+      Auto-hides Android/data and Android/obb package directories
+      from other apps via hash-table lookup at stat/open/readdir.
+
+config KSU_SUSFS_HARDENED
+    bool "Hardened SUSFS (upstream bug fixes)"
+    depends on KSU_SUSFS
+    default y
+    help
+      Fixes upstream SUSFS concurrency and error-handling bugs.
+      Disable to match exact upstream behavior.
diff -ruN post-upstream/fs/namei.c work-50/fs/namei.c
--- post-upstream/fs/namei.c	2026-03-05 15:17:39.735642225 +0100
+++ work-50/fs/namei.c	2026-03-08 14:20:56.905560259 +0100
@@ -39,6 +39,9 @@
 #include <linux/bitops.h>
 #include <linux/init_task.h>
 #include <linux/uaccess.h>
+#ifdef CONFIG_KSU_SUSFS
+#include <linux/susfs.h>
+#endif
 #if defined(CONFIG_KSU_SUSFS_SUS_PATH) || defined(CONFIG_KSU_SUSFS_OPEN_REDIRECT)
 #include <linux/susfs_def.h>
 #endif
@@ -1550,6 +1553,8 @@
 #ifdef CONFIG_KSU_SUSFS_SUS_PATH
 	{
 		if (!IS_ERR(dentry) && !found_sus_path && dentry->d_inode && susfs_is_inode_sus_path(dentry->d_inode)) {
+			if (d_in_lookup(dentry))
+				d_lookup_done(dentry);
 			dput(dentry);
 			dentry = lookup_dcache(&susfs_fake_qstr_name, base, flags);
 			found_sus_path = true;
@@ -1571,6 +1576,7 @@
 		goto skip_orig_flow;
 	}
 #endif
+
 	dentry = d_alloc(base, name);
 #ifdef CONFIG_KSU_SUSFS_SUS_PATH
 skip_orig_flow:
@@ -1608,7 +1614,9 @@
 		if (is_nd_state_lookup_last_and_open_last && dentry && !IS_ERR(dentry) && dentry->d_inode &&
 			susfs_is_inode_sus_path(dentry->d_inode))
 		{
-			dput(dentry);
+			if (d_in_lookup(dentry))
+				d_lookup_done(dentry);
+			// no dput() here, __d_lookup_rcu() does not take the dentry->d_lockref.count
 			dentry = NULL;
 		}
 #endif
@@ -1651,6 +1659,8 @@
 		if (is_nd_state_lookup_last_and_open_last && dentry && !IS_ERR(dentry) && dentry->d_inode &&
 			susfs_is_inode_sus_path(dentry->d_inode))
 		{
+			if (d_in_lookup(dentry))
+				d_lookup_done(dentry);
 			dput(dentry);
 			dentry = NULL;
 		}
@@ -1720,7 +1730,8 @@
 	if (is_nd_flags_lookup_last && !found_sus_path && dentry && !IS_ERR(dentry) && dentry->d_inode &&
 		susfs_is_inode_sus_path(dentry->d_inode))
 	{
-		d_lookup_done(dentry);
+		if (d_in_lookup(dentry))
+			d_lookup_done(dentry);
 		dput(dentry);
 		dentry = d_alloc_parallel(dir, &susfs_fake_qstr_name, &wq);
 		found_sus_path = true;
@@ -2308,6 +2319,7 @@
 		err = may_lookup(nd);
 		if (err)
 			return err;
+
 #ifdef CONFIG_KSU_SUSFS_SUS_PATH
 		dentry = nd->path.dentry;
 		if (dentry->d_inode && susfs_is_inode_sus_path(dentry->d_inode)) {
@@ -2316,7 +2328,6 @@
 			return -ENOENT;
 		}
 #endif
-
 		hash_len = hash_name(nd->path.dentry, name);
 
 		type = LAST_NORM;
@@ -3286,6 +3297,9 @@
 	if (is_nd_state_open_last && dentry && !IS_ERR(dentry) && dentry->d_inode &&
 		susfs_is_inode_sus_path(dentry->d_inode))
 	{
+		if (d_in_lookup(dentry)) {
+			d_lookup_done(dentry);
+		}
 		dput(dentry);
 		dentry = NULL;
 		found_sus_path = true;
@@ -3670,6 +3684,7 @@
 	struct file *filp;
 #ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
 	struct filename *fake_pathname;
+	struct inode *inode;
 #endif
 
 	set_nameidata(&nd, dfd, pathname);
@@ -3679,24 +3694,27 @@
 	if (unlikely(filp == ERR_PTR(-ESTALE)))
 		filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
 #ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
-	if (!IS_ERR(filp) &&
-		unlikely(test_bit(AS_FLAGS_OPEN_REDIRECT, &filp->f_inode->i_mapping->flags) &&
-		current_uid().val < 2000))
-	{
-		fake_pathname = susfs_get_redirected_path(filp->f_inode->i_ino);
-		if (!IS_ERR(fake_pathname)) {
-			restore_nameidata();
-			filp_close(filp, NULL);
-			// no need to do `putname(pathname);` here as it will be done by calling process
-			set_nameidata(&nd, dfd, fake_pathname);
-			filp = path_openat(&nd, op, flags | LOOKUP_RCU);
-			if (unlikely(filp == ERR_PTR(-ECHILD)))
-				filp = path_openat(&nd, op, flags);
-			if (unlikely(filp == ERR_PTR(-ESTALE)))
-				filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
-			restore_nameidata();
-			putname(fake_pathname);
-			return filp;
+	if (!IS_ERR(filp)) {
+		inode = file_inode(filp);
+		if (inode->i_mapping &&
+			unlikely(test_bit(AS_FLAGS_OPEN_REDIRECT, &inode->i_mapping->flags)) &&
+			current_uid().val < 2000)
+		{
+			fake_pathname = susfs_get_redirected_path(inode->i_ino);
+			if (!IS_ERR(fake_pathname)) {
+				restore_nameidata();
+				filp_close(filp, NULL);
+				// no need to do `putname(pathname);` here as it will be done by calling process
+				set_nameidata(&nd, dfd, fake_pathname);
+				filp = path_openat(&nd, op, flags | LOOKUP_RCU);
+				if (unlikely(filp == ERR_PTR(-ECHILD)))
+					filp = path_openat(&nd, op, flags);
+				if (unlikely(filp == ERR_PTR(-ESTALE)))
+					filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
+				restore_nameidata();
+				putname(fake_pathname);
+				return filp;
+			}
 		}
 	}
 #endif
@@ -3963,6 +3981,12 @@
 	int error;
 	unsigned int lookup_flags = LOOKUP_DIRECTORY;
 
+#ifdef CONFIG_KSU_SUSFS_UNICODE_FILTER
+	if (susfs_check_unicode_bypass(pathname)) {
+		return -ENOENT;
+	}
+#endif
+
 retry:
 	dentry = user_path_create(dfd, pathname, &path, lookup_flags);
 	if (IS_ERR(dentry))
@@ -4168,6 +4192,12 @@
 	struct inode *inode = NULL;
 	struct inode *delegated_inode = NULL;
 	unsigned int lookup_flags = 0;
+#ifdef CONFIG_KSU_SUSFS_UNICODE_FILTER
+	if (!IS_ERR(name) && susfs_check_unicode_bypass(name->uptr)) {
+		putname(name);
+		return -ENOENT;
+	}
+#endif
 retry:
 	name = filename_parentat(dfd, name, lookup_flags, &path, &last, &type);
 	if (IS_ERR(name))
@@ -4274,6 +4304,12 @@
 	struct path path;
 	unsigned int lookup_flags = 0;
 
+#ifdef CONFIG_KSU_SUSFS_UNICODE_FILTER
+	if (susfs_check_unicode_bypass(newname)) {
+		return -ENOENT;
+	}
+#endif
+
 	from = getname(oldname);
 	if (IS_ERR(from))
 		return PTR_ERR(from);
@@ -4405,6 +4441,12 @@
 	int how = 0;
 	int error;
 
+#ifdef CONFIG_KSU_SUSFS_UNICODE_FILTER
+	if (susfs_check_unicode_bypass(newname)) {
+		return -ENOENT;
+	}
+#endif
+
 	if ((flags & ~(AT_SYMLINK_FOLLOW | AT_EMPTY_PATH)) != 0)
 		return -EINVAL;
 	/*
@@ -4665,6 +4707,14 @@
 	bool should_retry = false;
 	int error = -EINVAL;
 
+#ifdef CONFIG_KSU_SUSFS_UNICODE_FILTER
+	if ((!IS_ERR(from) && susfs_check_unicode_bypass(from->uptr)) ||
+	    (!IS_ERR(to) && susfs_check_unicode_bypass(to->uptr))) {
+		error = -ENOENT;
+		goto put_both;
+	}
+#endif
+
 	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT))
 		goto put_both;
 
diff -ruN post-upstream/fs/namespace.c work-50/fs/namespace.c
--- post-upstream/fs/namespace.c	2026-03-05 15:17:39.736010897 +0100
+++ work-50/fs/namespace.c	2026-03-05 12:35:34.885192018 +0100
@@ -39,10 +39,9 @@
 
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 extern bool susfs_is_current_ksu_domain(void);
+extern bool susfs_is_current_zygote_domain(void);
 extern bool susfs_is_sdcard_android_data_decrypted __read_mostly;
 
-static atomic64_t susfs_ksu_mounts = ATOMIC64_INIT(0);
-
 #define CL_COPY_MNT_NS BIT(25) /* used by copy_mnt_ns() */
 #endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
@@ -126,20 +125,6 @@
 
 static void mnt_free_id(struct mount *mnt)
 {
-#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-	// First we have to check if susfs_mnt_id_backup == DEFAULT_KSU_MNT_ID,
-	// if so, no need to free.
-	if (mnt->mnt.susfs_mnt_id_backup == DEFAULT_KSU_MNT_ID) {
-		return;
-	}
-
-	// Second if susfs_mnt_id_backup was set after mnt_id reorder, free it if so.
-	if (likely(mnt->mnt.susfs_mnt_id_backup)) {
-		ida_free(&mnt_id_ida, mnt->mnt.susfs_mnt_id_backup);
-		return;
-	}
-
-#endif
 	ida_free(&mnt_id_ida, mnt->mnt_id);
 }
 
@@ -166,7 +151,7 @@
 bypass_orig_flow:
 #else
 	int res = ida_alloc_min(&mnt_group_ida, 1, GFP_KERNEL);
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 	if (res < 0)
 		return res;
@@ -217,18 +202,26 @@
 }
 
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-/* A copy of alloc_vfsmnt() but reuse the original mnt_id to mnt */
-static struct mount *susfs_reuse_sus_vfsmnt(const char *name, int orig_mnt_id)
+/* A copy of alloc_vfsmnt() but allocates the fake mnt_id for mounts
+ * that are unshared by ksu process
+ */
+static struct mount *susfs_alloc_unshare_ksu_vfsmnt(const char *name)
 {
 	struct mount *mnt = kmem_cache_zalloc(mnt_cache, GFP_KERNEL);
+	int res;
+
 	if (mnt) {
-		mnt->mnt_id = orig_mnt_id;
+		res = ida_alloc_min(&mnt_id_ida, DEFAULT_UNSHARE_KSU_MNT_ID, GFP_KERNEL);;
+		if (res < 0) {
+			goto out_free_cache;
+		}
+		mnt->mnt_id = res;
 
 		if (name) {
 			mnt->mnt_devname = kstrdup_const(name,
 							 GFP_KERNEL_ACCOUNT);
 			if (!mnt->mnt_devname)
-				goto out_free_cache;
+				goto out_free_id;
 		}
 
 #ifdef CONFIG_SMP
@@ -241,8 +234,6 @@
 		mnt->mnt_count = 1;
 		mnt->mnt_writers = 0;
 #endif
-		// Makes ida_free() easier to determine whether it should free the mnt_id or not
-		mnt->mnt.susfs_mnt_id_backup = DEFAULT_KSU_MNT_ID;
 
 		INIT_HLIST_NODE(&mnt->mnt_hash);
 		INIT_LIST_HEAD(&mnt->mnt_child);
@@ -262,25 +253,33 @@
 out_free_devname:
 	kfree_const(mnt->mnt_devname);
 #endif
+out_free_id:
+	mnt_free_id(mnt);
 out_free_cache:
 	kmem_cache_free(mnt_cache, mnt);
 	return NULL;
 }
-#endif
 
-#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-/* A copy of alloc_vfsmnt() but allocates the fake mnt_id to mnt */
-static struct mount *susfs_alloc_sus_vfsmnt(const char *name)
+/* A copy of alloc_vfsmnt() but allocates the fake mnt_id for mount
+ * that is mounted or single cloned by ksu process
+ */
+static struct mount *susfs_alloc_non_unshare_ksu_vfsmnt(const char *name)
 {
 	struct mount *mnt = kmem_cache_zalloc(mnt_cache, GFP_KERNEL);
+	int res;
+
 	if (mnt) {
-		mnt->mnt_id = DEFAULT_KSU_MNT_ID;
+		res = ida_alloc_min(&mnt_id_ida, DEFAULT_KSU_MNT_ID, GFP_KERNEL);;
+		if (res < 0) {
+			goto out_free_cache;
+		}
+		mnt->mnt_id = res;
 
 		if (name) {
 			mnt->mnt_devname = kstrdup_const(name,
 							 GFP_KERNEL_ACCOUNT);
 			if (!mnt->mnt_devname)
-				goto out_free_cache;
+				goto out_free_id;
 		}
 
 #ifdef CONFIG_SMP
@@ -293,8 +292,6 @@
 		mnt->mnt_count = 1;
 		mnt->mnt_writers = 0;
 #endif
-		// Makes ida_free() easier to determine whether it should free the mnt_id or not
-		mnt->mnt.susfs_mnt_id_backup = DEFAULT_KSU_MNT_ID;
 
 		INIT_HLIST_NODE(&mnt->mnt_hash);
 		INIT_LIST_HEAD(&mnt->mnt_child);
@@ -314,11 +311,13 @@
 out_free_devname:
 	kfree_const(mnt->mnt_devname);
 #endif
+out_free_id:
+	mnt_free_id(mnt);
 out_free_cache:
 	kmem_cache_free(mnt_cache, mnt);
 	return NULL;
 }
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 static struct mount *alloc_vfsmnt(const char *name)
 {
@@ -346,10 +345,6 @@
 		mnt->mnt_count = 1;
 		mnt->mnt_writers = 0;
 #endif
-#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-		// Make sure mnt->mnt.susfs_mnt_id_backup is initialized every time.
-		mnt->mnt.susfs_mnt_id_backup = 0;
-#endif
 
 		INIT_HLIST_NODE(&mnt->mnt_hash);
 		INIT_LIST_HEAD(&mnt->mnt_child);
@@ -1106,18 +1101,18 @@
 		return ERR_PTR(-EINVAL);
 
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-	// We keep checking for ksu process only until boot-completed stage is triggered
+	// - We will just stop checking for ksu process if /sdcard/Android is accessible,
+	//   for the sake of performance
 	if (!susfs_is_sdcard_android_data_decrypted && susfs_is_current_ksu_domain()) {
-		mnt = susfs_alloc_sus_vfsmnt(fc->source ?: "none");
-		atomic64_add(1, &susfs_ksu_mounts);
+		mnt = susfs_alloc_non_unshare_ksu_vfsmnt(fc->source ?: "none");
 		goto bypass_orig_flow;
 	}
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 	mnt = alloc_vfsmnt(fc->source ?: "none");
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 bypass_orig_flow:
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 	if (!mnt)
 		return ERR_PTR(-ENOMEM);
 
@@ -1201,36 +1196,38 @@
 	int err;
 
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-	// - We do not check anymore for ksu process if boot-completed stage is triggered
-	//   just to stop the performance loss
+	// - We will just stop checking for ksu process if /sdcard/Android is accessible,
+	//   for the sake of performance
 	if (susfs_is_sdcard_android_data_decrypted) {
 		goto skip_checking_for_ksu_proc;
 	}
 
-	// First we must check for ksu process because of magic mount
+	// - If /sdcard/Android is still not accessible, we keep checking for mounts
+	//   mounted by ksu process
 	if (susfs_is_current_ksu_domain()) {
-		// if it is unsharing, we reuse the old->mnt_id
+		// if it is unsharing, we assign the fake mnt_id starting with DEFAULT_UNSHARE_KSU_MNT_ID
 		if (flag & CL_COPY_MNT_NS) {
-			mnt = susfs_reuse_sus_vfsmnt(old->mnt_devname, old->mnt_id);
+			mnt = susfs_alloc_unshare_ksu_vfsmnt(old->mnt_devname);
 			goto bypass_orig_flow;
 		}
-		// else we just go assign fake mnt_id
-		mnt = susfs_alloc_sus_vfsmnt(old->mnt_devname);
+		// else we just go assign fake mnt_id starting with DEFAULT_KSU_MNT_ID
+		mnt = susfs_alloc_non_unshare_ksu_vfsmnt(old->mnt_devname);
 		goto bypass_orig_flow;
 	}
 
 skip_checking_for_ksu_proc:
-	// Lastly for other processes of which old->mnt_id == DEFAULT_KSU_MNT_ID, go assign fake mnt_id
-	if (old->mnt_id == DEFAULT_KSU_MNT_ID) {
-		mnt = susfs_alloc_sus_vfsmnt(old->mnt_devname);
+	// - We keep checking all processes and if old->mnt_id >= DEFAULT_KSU_MNT_ID,
+	//   go assign fake mnt_id starting with DEFAULT_KSU_MNT_ID
+	if (old->mnt_id >= DEFAULT_KSU_MNT_ID) {
+		mnt = susfs_alloc_non_unshare_ksu_vfsmnt(old->mnt_devname);
 		goto bypass_orig_flow;
 	}
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 	mnt = alloc_vfsmnt(old->mnt_devname);
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 bypass_orig_flow:
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 	if (!mnt)
 		return ERR_PTR(-ENOMEM);
 
@@ -3543,7 +3540,7 @@
 		copy_flags |= CL_SHARED_TO_SLAVE;
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 	copy_flags |= CL_COPY_MNT_NS;
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 	new = copy_tree(old, old->mnt.mnt_root, copy_flags);
 	if (IS_ERR(new)) {
 		namespace_unlock();
@@ -4339,40 +4336,5 @@
 };
 
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
-/* Reorder the mnt_id after all sus mounts are umounted during ksu_handle_setuid() */
-void susfs_reorder_mnt_id(void) {
-	struct mnt_namespace *mnt_ns = current->nsproxy->mnt_ns;
-	struct mount *mnt;
-	int first_mnt_id = 0;
-
-	// Do not reorder the mnt_id if there is no any ksu mount at all
-	if (atomic64_read(&susfs_ksu_mounts) == 0)
-		return;
-
-	down_read(&namespace_sem); // needed when manipulating mnt_namespace
-	lock_ns_list(mnt_ns); // needed when traversing mnt_ns->list
-	lock_mount_hash(); // needed when modifying mount
-
-	// - It is safe here as there should not be any first mnt with the sus mnt_id,
-	//   mount cloned by ksu proc is already handled in clone_mnt()
-	first_mnt_id = list_first_entry(&mnt_ns->list, struct mount, mnt_list)->mnt_id;
-	list_for_each_entry(mnt, &mnt_ns->list, mnt_list) {
-		// - We need to use mnt_is_cursor() to check if mnt is being looked up in
-		//   /proc/[mounts|mountinfo|mountstat], since mounts_open_common() will set 
-		//   the flag MNT_CURSOR on p->cursor.mnt.mnt_flags, skip it if so
-		if (mnt_is_cursor(mnt))
-			continue;
-		// It is very important that we don't reorder the sus mount if it is not umounted
-		if (mnt->mnt_id == DEFAULT_KSU_MNT_ID)
-			continue;
-		// We just still explicitly tell compiler not to optimizie this
-		WRITE_ONCE(mnt->mnt.susfs_mnt_id_backup, READ_ONCE(mnt->mnt_id));
-		WRITE_ONCE(mnt->mnt_id, first_mnt_id++);
-	}
-
-	unlock_mount_hash();
-	unlock_ns_list(mnt_ns);
-	up_read(&namespace_sem);
-}
+noinline void __used susfs_reorder_mnt_id(void) {}
 #endif
-
diff -ruN post-upstream/fs/notify/fdinfo.c work-50/fs/notify/fdinfo.c
--- post-upstream/fs/notify/fdinfo.c	2026-03-05 15:17:39.736250656 +0100
+++ work-50/fs/notify/fdinfo.c	2026-03-05 14:00:32.691465346 +0100
@@ -14,7 +14,7 @@
 #include <linux/exportfs.h>
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 #include <linux/susfs_def.h>
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 #include "inotify/inotify.h"
 #include "fdinfo.h"
@@ -33,7 +33,7 @@
 static void show_fdinfo(struct seq_file *m, struct file *f,
 			void (*show)(struct seq_file *m,
 				     struct fsnotify_mark *mark))
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 {
 	struct fsnotify_group *group = f->private_data;
 	struct fsnotify_mark *mark;
@@ -44,7 +44,7 @@
 		show(m, mark, f);
 #else
 		show(m, mark);
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 		if (seq_has_overflowed(m))
 			break;
 	}
@@ -90,13 +90,13 @@
 static void inotify_fdinfo(struct seq_file *m, struct fsnotify_mark *mark, struct file *file)
 #else
 static void inotify_fdinfo(struct seq_file *m, struct fsnotify_mark *mark)
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 {
 	struct inotify_inode_mark *inode_mark;
 	struct inode *inode;
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 	struct mount *mnt = NULL;
-#endif
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 
 	if (mark->connector->type != FSNOTIFY_OBJ_TYPE_INODE)
 		return;
@@ -106,36 +106,36 @@
 	if (inode) {
 #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 		mnt = real_mount(file->f_path.mnt);
-		if (likely(susfs_is_current_proc_umounted()) &&
-					mnt->mnt_id >= DEFAULT_KSU_MNT_ID)
+		if (mnt->mnt_id >= DEFAULT_KSU_MNT_ID &&
+			likely(susfs_is_current_proc_umounted_app()))
 		{
 			struct path path;
 			char *pathname = kmalloc(PAGE_SIZE, GFP_KERNEL);
 			char *dpath;
 			if (!pathname) {
-				goto out_seq_printf;
+				goto orig_flow;
 			}
 			dpath = d_path(&file->f_path, pathname, PAGE_SIZE);
-			if (!dpath) {
-				goto out_free_pathname;
+			if (IS_ERR(dpath)) {
+				kfree(pathname);
+				goto orig_flow;
 			}
 			if (kern_path(dpath, 0, &path)) {
-				goto out_free_pathname;
+				kfree(pathname);
+				goto orig_flow;
 			}
 			seq_printf(m, "inotify wd:%x ino:%lx sdev:%x mask:%x ignored_mask:0 ",
 					inode_mark->wd, path.dentry->d_inode->i_ino, path.dentry->d_inode->i_sb->s_dev,
 					inotify_mark_user_mask(mark));
 			show_mark_fhandle(m, path.dentry->d_inode);
 			seq_putc(m, '\n');
-			iput(inode);
 			path_put(&path);
 			kfree(pathname);
+			iput(inode);
 			return;
-out_free_pathname:
-			kfree(pathname);
 		}
-out_seq_printf:
-#endif
+orig_flow:
+#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
 		seq_printf(m, "inotify wd:%x ino:%lx sdev:%x mask:%x ignored_mask:0 ",
 			   inode_mark->wd, inode->i_ino, inode->i_sb->s_dev,
 			   inotify_mark_user_mask(mark));
diff -ruN post-upstream/fs/open.c work-50/fs/open.c
--- post-upstream/fs/open.c	2026-03-05 15:17:39.736324401 +0100
+++ work-50/fs/open.c	2026-03-08 06:33:30.557822980 +0100
@@ -33,6 +33,9 @@
 #include <linux/dnotify.h>
 #include <linux/compat.h>
 #ifdef CONFIG_KSU_SUSFS
+#include <linux/susfs.h>
+#endif
+#ifdef CONFIG_KSU_SUSFS
 #include <linux/susfs_def.h>
 #endif
 
@@ -403,6 +406,10 @@
 extern bool __ksu_is_allow_uid_for_current(uid_t uid);
 extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
 			int *flags);
+extern bool susfs_is_current_proc_umounted(void);
+#ifdef CONFIG_KSU_SUSFS_HIDDEN_NAME
+extern bool susfs_is_hidden_name(const char *name, int namlen, uid_t caller_uid);
+#endif
 #endif
 
 static long do_faccessat(int dfd, const char __user *filename, int mode, int flags)
@@ -447,6 +454,28 @@
 	if (res)
 		goto out;
 
+#ifdef CONFIG_KSU_SUSFS_HIDDEN_NAME
+	if (current_uid().val >= 10000 &&
+	    susfs_is_current_proc_umounted()) {
+		struct dentry *_
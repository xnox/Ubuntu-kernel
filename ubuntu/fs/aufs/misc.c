/*
 * Copyright (C) 2005, 2006, 2007 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* $Id: misc.c,v 1.47 2007/11/12 01:40:06 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
//#include <linux/mm.h>
//#include <asm/uaccess.h>
#include "aufs.h"

void *au_kzrealloc(void *p, unsigned int nused, unsigned int new_sz, gfp_t gfp)
{
	void *q;

	LKTRTrace("p %p, nused %d, sz %d\n", p, nused, new_sz);
	AuDebugOn(new_sz <= 0);
	if (new_sz <= nused)
		return p;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
	q = krealloc(p, new_sz, gfp);
	if (q)
		memset(q + nused, 0, new_sz - nused);
	return q;
#else
	LKTRTrace("ksize %d\n", ksize(p));
	if (new_sz <= ksize(p)) {
		memset(p + nused, 0, new_sz - nused);
		return p;
	}

	q = kmalloc(new_sz, gfp);
	//q = NULL;
	if (unlikely(!q))
		return NULL;
	memcpy(q, p, nused);
	memset(q + nused, 0, new_sz - nused);
	//smp_mb();
	kfree(p);
	return q;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) */
}

/* ---------------------------------------------------------------------- */

struct nameidata *au_dup_nd(struct aufs_sbinfo *sbinfo, struct nameidata *dst,
			    struct nameidata *src)
{
	LKTRTrace("src %p\n", src);

	if (src) {
		*dst = *src;
		dst->flags &= ~LOOKUP_PARENT;
		if (unlikely(AuFlag(sbinfo, f_wbr_create) != AuWbrCreate_TDP)) {
			dst->flags &= ~LOOKUP_CREATE;
			dst->intent.open.flags &= ~O_CREAT;
		}
	} else
		dst = NULL;

	return dst;
}

// todo: make it inline
struct nameidata *au_fake_dm(struct nameidata *fake_nd, struct nameidata *nd,
			     struct super_block *sb, aufs_bindex_t bindex)
{
	LKTRTrace("nd %p, b%d\n", nd, bindex);

	if (!nd)
		return NULL;

	fake_nd->dentry = NULL;
	fake_nd->mnt = NULL;

#ifndef CONFIG_AUFS_FAKE_DM
	DiMustAnyLock(nd->dentry);

	if (bindex <= dbend(nd->dentry))
		fake_nd->dentry = au_h_dptr_i(nd->dentry, bindex);
	if (fake_nd->dentry) {
		dget(fake_nd->dentry);
		fake_nd->mnt = sbr_mnt(sb, bindex);
		AuDebugOn(!fake_nd->mnt);
		mntget(fake_nd->mnt);
	} else
		fake_nd = ERR_PTR(-ENOENT);
#endif

	AuTraceErrPtr(fake_nd);
	return fake_nd;
}

void au_fake_dm_release(struct nameidata *fake_nd)
{
#ifndef CONFIG_AUFS_FAKE_DM
	if (fake_nd) {
		mntput(fake_nd->mnt);
		dput(fake_nd->dentry);
	}
#endif
}

int au_h_create(struct inode *h_dir, struct dentry *h_dentry, int mode,
		int dlgt, struct nameidata *nd, struct vfsmount *nfsmnt)
{
	int err;

	LKTRTrace("hi%lu, %.*s, 0%o, nd %d, nfsmnt %d\n",
		  h_dir->i_ino, AuDLNPair(h_dentry), mode, !!nd, !!nfsmnt);

	err = -ENOSYS;
	if (!nfsmnt)
		err = vfsub_create(h_dir, h_dentry, mode, /*nd*/NULL, dlgt);
	else {
#ifndef CONFIG_AUFS_FAKE_DM
		struct nameidata fake_nd;

		if (nd)
			fake_nd = *nd;
		else
			memset(&fake_nd, 0, sizeof(fake_nd));
		fake_nd.dentry = dget(h_dentry);
		fake_nd.mnt = mntget(nfsmnt);
		fake_nd.flags = LOOKUP_CREATE;
		fake_nd.intent.open.flags = O_CREAT | FMODE_READ;
		fake_nd.intent.open.create_mode = mode;

		err = vfsub_create(h_dir, h_dentry, mode, &fake_nd, dlgt);
		path_release(&fake_nd);
#endif
	}

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int au_copy_file(struct file *dst, struct file *src, loff_t len,
		 struct super_block *sb)
{
	int err, all_zero;
	unsigned long blksize;
	char *buf;
	struct vfsub_args vargs;
	/* reduce stack space */
	struct iattr *ia;

	LKTRTrace("%.*s, %.*s\n",
		  AuDLNPair(dst->f_dentry), AuDLNPair(src->f_dentry));
	AuDebugOn(!(dst->f_mode & FMODE_WRITE));
#ifdef CONFIG_AUFS_DEBUG
	{
		struct dentry *parent;
		parent = dget_parent(dst->f_dentry);
		IMustLock(parent->d_inode);
		dput(parent);
	}
#endif

	err = -ENOMEM;
	blksize = dst->f_dentry->d_sb->s_blocksize;
	if (!blksize || PAGE_SIZE < blksize)
		blksize = PAGE_SIZE;
	LKTRTrace("blksize %lu\n", blksize);
	buf = kmalloc(blksize, GFP_TEMPORARY);
	//buf = NULL;
	if (unlikely(!buf))
		goto out;
	ia = kmalloc(sizeof(*ia), GFP_TEMPORARY);
	if (unlikely(!ia))
		goto out_buf;

#ifdef CONFIG_AUFS_DEBUG
	if (len > (1 << 22))
		AuWarn("copying a large file %Ld\n", (long long)len);
#endif
	vfsub_args_init(&vargs, NULL, au_need_dlgt(sb), 0);
	err = 0;
	all_zero = 0;
	src->f_pos = 0;
	dst->f_pos = 0;
	while (len) {
		size_t sz, rbytes, wbytes, i;
		char *p;

		LKTRTrace("len %lld\n", len);
		sz = blksize;
		if (len < blksize)
			sz = len;

		/* support LSM and notify */
		rbytes = 0;
		// signal_pending
		while (!rbytes || err == -EAGAIN || err == -EINTR) {
			rbytes = vfsub_read_k(src, buf, sz, &src->f_pos,
					      vargs.dlgt);
			err = rbytes;
		}
		if (unlikely(err < 0))
			break;

		all_zero = 0;
		if (len >= rbytes && rbytes == blksize) {
			all_zero = 1;
			p = buf;
			for (i = 0; all_zero && i < rbytes; i++)
				all_zero = !*p++;
		}
		if (!all_zero) {
			wbytes = rbytes;
			p = buf;
			while (wbytes) {
				size_t b;
				/* support LSM and notify */
				b = vfsub_write_k(dst, p, wbytes, &dst->f_pos,
						  &vargs);
				err = b;
				// signal_pending
				if (unlikely(err == -EAGAIN || err == -EINTR))
					continue;
				if (unlikely(err < 0))
					break;
				wbytes -= b;
				p += b;
			}
		} else {
			loff_t res;
			LKTRLabel(hole);
			res = vfsub_llseek(dst, rbytes, SEEK_CUR);
			err = res;
			if (unlikely(res < 0))
				break;
		}
		len -= rbytes;
		err = 0;
	}

	/* the last block may be a hole */
	if (unlikely(!err && all_zero)) {
		struct dentry *h_d = dst->f_dentry;
		struct inode *h_i = h_d->d_inode;

		LKTRLabel(last hole);
		do {
			// signal_pending
			err = vfsub_write_k(dst, "\0", 1, &dst->f_pos, &vargs);
		} while (err == -EAGAIN || err == -EINTR);
		if (err == 1) {
			ia->ia_size = dst->f_pos;
			#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) && defined(CONFIG_SECURITY_APPARMOR)
			ia->ia_valid = ATTR_SIZE;
			#else
			ia->ia_valid = ATTR_SIZE | ATTR_FILE;
			ia->ia_file = dst;
			#endif
			vfsub_i_lock_nested(h_i, AuLsc_I_CHILD2);
			err = vfsub_notify_change(h_d, ia, &vargs);
			vfsub_i_unlock(h_i);
		}
	}

	kfree(ia);
 out_buf:
	kfree(buf);
 out:
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int au_test_ro(struct super_block *sb, aufs_bindex_t bindex,
	       struct inode *inode)
{
	int err;

	err = br_rdonly(stobr(sb, bindex));

	/* pseudo-link after flushed may out of bounds */
	if (!err
	    && inode
	    && ibstart(inode) <= bindex
	    && bindex <= ibend(inode)) {
		/*
		 * permission check is unnecessary since later vfsub routine
		 * does
		 */
		struct inode *hi = au_h_iptr_i(inode, bindex);
		if (hi)
			err = IS_IMMUTABLE(hi) ? -EROFS : 0;
	}

	AuTraceErr(err);
	return err;
}

int au_test_perm(struct inode *hidden_inode, int mask, int dlgt)
{
	if (!current->fsuid)
		return 0;
	if (unlikely(au_test_nfs(hidden_inode->i_sb)
		     && (mask & MAY_WRITE)
		     && S_ISDIR(hidden_inode->i_mode)))
		mask |= MAY_READ; /* force permission check */
	//todo: fake nameidata
	return vfsub_permission(hidden_inode, mask, NULL, dlgt);
}

/***********************************************************************
 * "seq" files template
 *
 * This header works with "linux/seq_file.h"
 *
 * Licenced under GPL.
 *
 * Copyright (C) YOKOTA Hiroshi <yokota (at) netlab. is. tsukuba. ac. jp>
 */

#ifndef _LINUX_SEQ_TEMPLATE_H_
#define _LINUX_SEQ_TEMPLATE_H_


/*************************************************************************
 * "seq" file template definition.
 */
/* "seq" initializer */
#define SEQ_OPEN_FS(_open_func_name_, _show_func_name_) \
static int _open_func_name_(struct inode *inode, struct file *file) \
{								      \
	return single_open(file, _show_func_name_, PDE(inode)->data);  \
}

/*-------------------------------------------------------------------------
 * "seq" fops template for read-only files.
 */
#define SEQ_FILEOPS_R(_open_func_name_) \
{ \
	.open	 = _open_func_name_,		  \
	.read	 = seq_read,			  \
	.llseek	 = seq_lseek,			  \
	.release = single_release,		  \
}

/*------------------------------------------------------------------------
 * "seq" fops template for read-write files.
 */
#define SEQ_FILEOPS_RW(_open_func_name_, _write_func_name_) \
{ \
	.open	 = _open_func_name_ ,		  \
	.read	 = seq_read,			  \
	.write	 = _write_func_name_,		  \
	.llseek	 = seq_lseek,			  \
	.release = single_release,		  \
}

#endif
/* end */

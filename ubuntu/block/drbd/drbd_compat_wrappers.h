/*
 * FIXME this file is bound to die, renamed or included in drbd_int.h
 */

#include <linux/version.h>


/* struct page has a union in 2.6.15 ...
 * an anonymous union and struct since 2.6.16
 * or in fc5 "2.6.15" */
#include <linux/mm.h>
#ifndef page_private
# define page_private(page)		((page)->private)
# define set_page_private(page, v)	((page)->private = (v))
#endif

#include <linux/buffer_head.h> // for fsync_bdev

/* see get_sb_bdev and bd_claim */
extern char* drbd_sec_holder;

static inline sector_t drbd_get_hardsect(struct block_device *bdev)
{
	return bdev->bd_disk->queue->hardsect_size;
}

/* Returns the number of 512 byte sectors of the device */
static inline sector_t drbd_get_capacity(struct block_device *bdev)
{
	/* return bdev ? get_capacity(bdev->bd_disk) : 0; */
	return bdev ? bdev->bd_inode->i_size >> 9 : 0;
}

/* sets the number of 512 byte sectors of our virtual device */
static inline void drbd_set_my_capacity(drbd_dev *mdev,
					sector_t size)
{
	/* set_capacity(mdev->this_bdev->bd_disk, size); */
	set_capacity(mdev->vdisk,size);
	mdev->this_bdev->bd_inode->i_size = (loff_t)size << 9;
}

static inline int drbd_sync_me(drbd_dev *mdev)
{
	return fsync_bdev(mdev->this_bdev);
}

#define drbd_bio_uptodate(bio) bio_flagged(bio,BIO_UPTODATE)

#ifdef CONFIG_HIGHMEM
/*
 * I don't know why there is no bvec_kmap, only bvec_kmap_irq ...
 *
 * we do a sock_recvmsg into the target buffer,
 * so we obviously cannot use the bvec_kmap_irq variant.	-lge
 *
 * Most likely it is only due to performance anyways:
  * kmap_atomic/kunmap_atomic is significantly faster than kmap/kunmap because
  * no global lock is needed and because the kmap code must perform a global TLB
  * invalidation when the kmap pool wraps.
  *
  * However when holding an atomic kmap is is not legal to sleep, so atomic
  * kmaps are appropriate for short, tight code paths only.
 */
static inline char *drbd_bio_kmap(struct bio *bio)
{
	struct bio_vec *bvec = bio_iovec(bio);
	unsigned long addr;

	addr = (unsigned long) kmap(bvec->bv_page);

	if (addr & ~PAGE_MASK)
		BUG();

	return (char *) addr + bvec->bv_offset;
}

static inline void drbd_bio_kunmap(struct bio *bio)
{
	struct bio_vec *bvec = bio_iovec(bio);

	kunmap(bvec->bv_page);
}

#else
static inline char *drbd_bio_kmap(struct bio *bio)
{
	struct bio_vec *bvec = bio_iovec(bio);
	return page_address(bvec->bv_page) + bvec->bv_offset;
}
static inline void drbd_bio_kunmap(struct bio *bio)
{
	// do nothing.
}
#endif

static inline int drbd_bio_has_active_page(struct bio *bio)
{
	struct bio_vec *bvec;
	int i;

	__bio_for_each_segment(bvec, bio, i, 0) {
		if (page_count(bvec->bv_page) > 1) return 1;
	}

	return 0;
}

#define BIO_ENDIO_FN(name) void name(struct bio *bio, int error)
#define BIO_ENDIO_FN_START while(0) {}
#define BIO_ENDIO_FN_RETURN return

// bi_end_io handlers
extern BIO_ENDIO_FN(drbd_md_io_complete);
extern BIO_ENDIO_FN(drbd_endio_read_sec);
extern BIO_ENDIO_FN(drbd_endio_write_sec);
extern BIO_ENDIO_FN(drbd_endio_pri);


/*
 * used to submit our private bio
 */
static inline void drbd_generic_make_request(drbd_dev *mdev, int rw, int fault_type, struct bio *bio)
{
	bio->bi_rw = rw; // on the receiver side, e->..rw was not yet defined.

	if (!bio->bi_bdev) {
		printk(KERN_ERR DEVICE_NAME "%d: drbd_generic_make_request: bio->bi_bdev == NULL\n",
		       mdev_to_minor(mdev));
		dump_stack();
		bio_endio(bio, -ENODEV);
		return;
	}

	if (FAULT_ACTIVE(mdev, fault_type))
		bio_endio(bio, -EIO);
	else
		generic_make_request(bio);
}

static inline void drbd_plug_device(drbd_dev *mdev)
{
	struct request_queue *q;
	q = bdev_get_queue(mdev->this_bdev);

	spin_lock_irq(q->queue_lock);

/* XXX the check on !blk_queue_plugged is redundant,
 * implicitly checked in blk_plug_device */

	if(!blk_queue_plugged(q)) {
		blk_plug_device(q);
		del_timer(&q->unplug_timer);
		// unplugging should not happen automatically...
	}
	spin_unlock_irq(q->queue_lock);
}

static inline int _drbd_send_bio(drbd_dev *mdev, struct bio *bio)
{
	struct bio_vec *bvec = bio_iovec(bio);
	struct page *page = bvec->bv_page;
	size_t size = bvec->bv_len;
	int offset = bvec->bv_offset;
	int ret;

	ret = drbd_send(mdev, mdev->data.socket, kmap(page) + offset, size, 0);
	kunmap(page);
	return ret;
}

#ifdef DEFINE_SOCK_CREATE_KERN
#define sock_create_kern sock_create
#endif

#ifdef USE_KMEM_CACHE_S
#define kmem_cache kmem_cache_s
#endif

#define drbd_unregister_blkdev unregister_blkdev

#ifdef NEED_BACKPORT_OF_ATOMIC_ADD

#if defined(__x86_64__)

static __inline__ int atomic_add_return(int i, atomic_t *v)
{
        int __i = i;
        __asm__ __volatile__(
                LOCK_PREFIX "xaddl %0, %1;"
                :"=r"(i)
                :"m"(v->counter), "0"(i));
        return i + __i;
}

static __inline__ int atomic_sub_return(int i, atomic_t *v)
{
        return atomic_add_return(-i,v);
}

#define atomic_inc_return(v)  (atomic_add_return(1,v))
#define atomic_dec_return(v)  (atomic_sub_return(1,v))

#elif defined(__i386__) || defined(__arch_um__)

static __inline__ int atomic_add_return(int i, atomic_t *v)
{
        int __i;
#ifdef CONFIG_M386
        unsigned long flags;
        if(unlikely(boot_cpu_data.x86==3))
                goto no_xadd;
#endif
        /* Modern 486+ processor */
        __i = i;
        __asm__ __volatile__(
                LOCK_PREFIX "xaddl %0, %1;"
                :"=r"(i)
                :"m"(v->counter), "0"(i));
        return i + __i;

#ifdef CONFIG_M386
no_xadd: /* Legacy 386 processor */
        local_irq_save(flags);
        __i = atomic_read(v);
        atomic_set(v, i + __i);
        local_irq_restore(flags);
        return i + __i;
#endif
}

static __inline__ int atomic_sub_return(int i, atomic_t *v)
{
        return atomic_add_return(-i,v);
}

#define atomic_inc_return(v)  (atomic_add_return(1,v))
#define atomic_dec_return(v)  (atomic_sub_return(1,v))

#else
# error "You need to copy/past atomic_inc_return()/atomic_dec_return() here"
# error "for your architecture. (Hint: Kernels after 2.6.10 have those"
# error "by default! Using a later kernel might be less effort!)"
#endif

#endif

#ifdef NEED_BACKPORT_OF_KZALLOC
static inline void *kzalloc(size_t size, int flags)
{
	void *rv = kmalloc(size,flags);
	if(rv) memset(rv,0,size);

	return rv;
}
#endif

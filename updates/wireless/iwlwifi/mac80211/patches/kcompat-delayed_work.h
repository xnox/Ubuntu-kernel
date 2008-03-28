#ifndef __kcompat_delayed_work_h__
#define __kcompat_delayed_work_h__

/* Out-of-tree / in-tree kernel compatibility */
#include <linux/version.h>
struct delayed_work {
	struct work_struct work;
};
#define INIT_DELAYED_WORK(_work, _func) \
        INIT_WORK(&(_work)->work, _func)
#undef INIT_WORK

#define INIT_WORK(_work, _func)                                 \
	do {							\
		INIT_LIST_HEAD(&(_work)->entry);		\
		(_work)->pending = 0;				\
		PREPARE_WORK((_work), (void *)(_func), (_work));\
		init_timer(&(_work)->timer);			\
	} while (0)

static inline int compat_schedule_delayed_work(
	struct delayed_work *work, unsigned long delay)
{
	return schedule_delayed_work(&work->work, delay);
}

static inline int compat_queue_delayed_work(struct workqueue_struct *queue,
	struct delayed_work *work, unsigned long delay)
{
	return queue_delayed_work(queue, &work->work, delay);
}

static inline int compat_cancel_delayed_work(
	struct delayed_work *work)
{
	return cancel_delayed_work(&work->work);
}

#endif

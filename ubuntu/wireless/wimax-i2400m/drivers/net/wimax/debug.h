/*
 * Intel Wireless WiMAX Connection 2400m
 * Collection of tools to manage debug operations.
 *
 *
 * Copyright (C) 2005-2007 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Don't #include this file directly, read on!
 *
 *
 * EXECUTING DEBUGGING ACTIONS OR NOT
 *
 * The main thing this framework provides you with is decission power
 * to take a debug action (like printing a message) if the current
 * debug level allows it.
 *
 * There are two levels of granularity for selecting this:
 *
 *  - Master switch                         [compile time, per module]
 *  - Runtime per .c file                   [run time, per submodule]
 *
 * A call to d_test(L) (L being the target debug level) returns true
 * if the action should be taken because the current debug levels
 * allow it.
 *
 * It is done in such a way that a call to d_test() that can be
 * determined to be always false at compile time will get the code
 * depending on it compiled out by optimization.
 *
 *
 * DEBUG LEVELS
 *
 * It is up to the caller to define how much a debugging level is.
 *
 * I usually set 0 as "no debug" (so an action marked as debug level 0
 * will always be taken). The increasing debug levels are used for
 * increased verbosity.
 *
 *
 * USAGE
 *
 * Group your code in submodules and your submodules in modules [which
 * in most cases want to map to Linux modules and .c files that
 * compose those].
 *
 *
 * For each module, you will have:
 *
 *  - a debug-levels.h header file that declares the list of
 *    submodules and that is included by all .c files that use
 *    the debugging tools.
 *  - some .c code to manipulate the runtime debug levels (through
 *    sysfs). You will have to attach this to some sysfs point.
 *
 * Your debug-levels.h (or any other name) file might look like:
 *
 *
 *     #ifndef __debug_levels__h__
 *     #define __debug_levels__h__
 *
 *     #define D_MASTER 10
 *
 *     #include ".../whereveritis/.../debug.h"
 *
 *     enum d_module {
 *             D_SUBMODULE_DECLARE(submodule_1),
 *             D_SUBMODULE_DECLARE(submodule_2),
 *             ...
 *             D_SUBMODULE_DECLARE(submodule_N)
 *     };
 *
 *     #endif
 *
 * D_MASTER is the maximum compile-time debug level; any debug actions
 * above this will be likely compiled out.
 *
 * We declare N different submodules whose debug level can be
 * independently controlled.
 *
 * In a .c file of the module (and only in one of them), define the
 * following code:
 *
 *     struct d_level d_level[] = {
 *             D_SUBMODULE_DEFINE(submodule_1),
 *             D_SUBMODULE_DEFINE(submodule_2),
 *             ...
 *             D_SUBMODULE_DEFINE(submodule_N),
 *     };
 *     size_t d_level_size = ARRAY_SIZE(d_level);
 *
 *     static
 *     DEVICE_ATTR(debug_levels, S_IRUGO | S_IWUSR,
 *                 d_level_show, d_level_store);
 *
 * Externs for d_level and d_level_size are used and declared in this
 * file, debug.h.
 *
 * Of course, you can manipulate the levels using other mechanisms;
 * d_level_show() and d_level_store() are provided for
 * convenience. However, be aware that even if this will show attached
 * to some particular instance of a device, the settings are *global*.
 *
 *
 * For each submodule (for example, .c files), invoke like:
 *
 *     #define D_SUBMODULE submodule_x     // matches one in debug-levels.h
 *     #include "debug-levels.h"
 *
 * after #including all your include files.
 *
 *
 * Now you can use the d_*() macros below [d_test(), d_fnstart(),
 * d_fnend(), d_printf(), d_dump()].
 *
 * If their debug level is greater than D_MASTER, they will be
 * compiled out.
 *
 * If their debug level is lower or equal than D_MASTER but greater
 * than the current debug level of their submodule, they'll be
 * ignored.
 *
 * Otherwise, the action will be performed.
 */
#ifndef __debug__h__
#define __debug__h__

#include <linux/types.h>
#include <linux/device.h>


/* Backend stuff */

/*
 * Debug backend: generate a message header from a 'struct device'
 *
 * @head: buffer where to place the header
 * @head_size: length of @head
 * @dev: pointer to device used to generate a header from. If NULL,
 *     an empty ("") header is generated.
 */
static inline
void __d_head(char *head, size_t head_size,
	      struct device *dev)
{
	if (dev == NULL)
		head[0] = 0;
	else if ((unsigned long)dev < 4096) {
		printk(KERN_ERR "E: Corrupt dev %p\n", dev);
		WARN_ON(1);
	} else
		snprintf(head, head_size, "%s %s: ",
			 dev_driver_string(dev), dev->bus_id);
}


/*
 * Debug backend: log some message if debugging is enabled
 *
 * @l: intended debug level
 * @tag: tag to prefix the message with
 * @dev: 'struct device' associated to this message
 * @f: printf-like format and arguments
 */
#define _d_printf(l, tag, dev, f, a...)					\
do {									\
	char head[64];							\
	if (!d_test(l))							\
		break;							\
	__d_head(head, sizeof(head), dev);				\
	printk(KERN_ERR "%s%s%s: " f, head, __func__, tag, ##a);	\
} while (0 && dev)


/*
 * CPP sintatic sugar to get the __D_SUBMODULE_X symbol when the
 * argument (_name) is a preprocessor #define.
 */
#define _D_SUBMODULE_INDEX(_name) (D_SUBMODULE_DECLARE(_name))


/*
 * Store a submodule's runtime debug level and name
 */
struct d_level {
	unsigned level;
	const char *name;
};

/*
 * List of available submodules and their debug levels
 */
extern struct d_level d_level[];
extern size_t d_level_size;


/*
 * Frontend stuff
 *
 *
 * Stuff you need to declare prior to using the actual "debug" actions
 * (defined below).
 */

#ifndef D_MASTER
#error D_MASTER not defined, but debug.h included! [see docs]
/**
 * D_MASTER - Compile time maximum debug level
 *
 * #define in your debug-levels.h file to the maximum debug level the
 * runtime code will be allowed to have. This allows you to provide a
 * main knob.
 *
 * Anything above that level will be optimized out of the compile.
 *
 * Defaults to the maximum allowable debug level.
 *
 * Maximum one definition per module (at the debug-levels.h file).
 */
#define D_MASTER ~0
#endif


#ifndef D_SUBMODULE
#error D_SUBMODULE not defined, but debug.h included! [see docs]
/**
 * D_SUBMODULE - Name of the current submodule
 *
 * #define in your submodule .c file before #including debug-levels.h
 * to the name of the current submodule as previously declared and
 * defined with D_SUBMODULE_DECLARE() (in your module's
 * debug-levels.h) and D_SUBMODULE_DEFINE().
 *
 * This is used to provide runtime-control over the debug levels.
 *
 * Maximum one per .c file! Can be shared among different .c files
 * (meaning they belong to the same submodule categorization).
 */
#define D_SUBMODULE undefined_module
#endif


/**
 * D_SUBMODULE_DECLARE - Declare a submodule for runtime debug level control
 *
 * @_name: name of the submodule, restricted to the chars that make up a
 *     valid C identifier ([a-zA-Z0-9_]).
 *
 * Declare in your module's debug-levels.h header file as:
 *
 * enum d_module {
 *         D_SUBMODULE_DECLARE(submodule_1),
 *         D_SUBMODULE_DECLARE(submodule_2),
 *         D_SUBMODULE_DECLARE(submodule_3),
 *         D_SUBMODULE_DECLARE(NUMBER),		// Keep this one at bottom
 * };
 *
 * Some corresponding .c file needs to have a matching
 * D_SUBMODULE_DEFINE().
 */
#define D_SUBMODULE_DECLARE(_name) __D_SUBMODULE_##_name


/**
 * D_SUBMODULE_DEFINE - Define a submodule for runtime debug level control
 *
 * @_name: name of the submodule, restricted to the chars that make up a
 *     valid C identifier ([a-zA-Z0-9_]).
 *
 * Use once per module (in some .c file) as:
 *
 * static
 * struct d_level d_level[] = {
 *         D_SUBMODULE_DEFINE(submodule_1),
 *         D_SUBMODULE_DEFINE(submodule_2),
 *         D_SUBMODULE_DEFINE(submodule_3),
 *         D_SUBMODULE_DEFINE(NUMBER),		// Keep this one at bottom
 * };
 *
 * Matching D_SUBMODULE_DECLARE()s have to be present somewhere (normally
 * in a header file).
 */
#define D_SUBMODULE_DEFINE(_name)			\
[__D_SUBMODULE_##_name] = {			\
	.level = 0,				\
	.name = #_name				\
}



/* The actual "debug" operations */


/**
 * d_test - Returns true if debugging should be enabled
 *
 * @l: intended debug level (unsigned)
 *
 * If the master debug switch is enabled and the current settings are
 * higher or equal to the requested level, then debugging
 * output/actions should be enabled.
 *
 * NOTE:
 *
 * This needs to be coded so that it can be evaluated in compile
 * time; this is why the ugly BUG_ON() is placed in there, so the
 * D_MASTER evaluation compiles all out if it is compile-time false.
 */
#define d_test(l)							\
({									\
	unsigned __l = l;	/* type enforcer */			\
	(D_MASTER) >= __l						\
	&& ({								\
		BUG_ON(_D_SUBMODULE_INDEX(D_SUBMODULE) >= d_level_size); \
		d_level[_D_SUBMODULE_INDEX(D_SUBMODULE)].level >= __l;	\
	});								\
})


/**
 * d_fnstart - log message at function start if debugging enabled
 *
 * @l: intended debug level
 * @_dev: 'struct device' pointer, NULL if none (for context)
 * @f: printf-like format and arguments
 */
#define d_fnstart(l, _dev, f, a...) _d_printf(l, " FNSTART", _dev, f, ## a)


/**
 * d_fnend - log message at function end if debugging enabled
 *
 * @l: intended debug level
 * @_dev: 'struct device' pointer, NULL if none (for context)
 * @f: printf-like format and arguments
 */
#define d_fnend(l, _dev, f, a...) _d_printf(l, " FNEND", _dev, f, ## a)


/**
 * d_printf - log message if debugging enabled
 *
 * @l: intended debug level
 * @_dev: 'struct device' pointer, NULL if none (for context)
 * @f: printf-like format and arguments
 */
#define d_printf(l, _dev, f, a...) _d_printf(l, "", _dev, f, ## a)


/**
 * d_printf - log buffer hex dump if debugging enabled
 *
 * @l: intended debug level
 * @_dev: 'struct device' pointer, NULL if none (for context)
 * @f: printf-like format and arguments
 */
#define d_dump(l, dev, ptr, size)			\
do {							\
	char head[64];					\
	if (!d_test(l))					\
		break;					\
	__d_head(head, sizeof(head), dev);		\
	print_hex_dump(KERN_ERR, head, 0, 16, 1,	\
		       ((void *) ptr), (size), 0);	\
} while (0 && dev)




/*
 * Exporting the runtime debug levels via sysfs
 *
 * Use d_level_{store,show}() manipulators below to declare the
 * following:
 *
 *
 * struct d_level d_level[] = {
 *         D_MODULE_DEFINE(submodule_1),
 *         D_MODULE_DEFINE(submodule_2),
 * };
 * size_t d_level_size = ARRAY_SIZE(d_level);
 *
 * static
 * DEVICE_ATTR(debug_levels, S_IRUGO | S_IWUSR, d_level_show, d_level_store);
 *
 * Then register the device attribute file with your 'struct device'.
 *
 * Note these are attached to a device-specific struct, but in all
 * thruth, they are global to all the devices. So if you increase
 * debugging through the deviceX's sysfs controls, you increase it for
 * all of them.
 *
 * There is no good "global" place to attach them (the module doesn't
 * exist if you compile it in, for example). For now, it does the
 * trick.
 */


/*
 * Shows a list of the module name and current debug level
 */
static inline
ssize_t d_level_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	ssize_t result = 0;
	unsigned itr;

	for (itr = 0; itr < d_level_size; itr++)
		result += scnprintf(buf + result, PAGE_SIZE - result,
				    "%u %s\n",
				    d_level[itr].level, d_level[itr].name);
	return result;
}


/*
 * Takes a (new) log level and module name and sets it
 */
static inline
ssize_t d_level_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t size)
{
	ssize_t result;
	unsigned level, itr;
	char name[32];

	result = -EINVAL;
	if (sscanf(buf, "%u %31s\n", &level, name) != 2) {
		dev_err(dev, "error: expecting 'LEVEL SUBMODULENAME'; "
			"can't parse '%s'\n", buf);
		goto error_bad_values;
	}
	for (itr = 0; itr < d_level_size; itr++)
		if (!strcmp(d_level[itr].name, name)) {
			d_level[itr].level = level;
			return size;
		}
	dev_err(dev, "error: trying to set debug level for "
		"unknown debug submodule '%s'\n", name);
	result = -ENXIO;
error_bad_values:
	return result;
}


#endif /* #ifndef __debug__h__ */

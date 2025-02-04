/* Copyright (c) 2010-2015, 2017-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/rq_stats.h>

#define MAX_LONG_SIZE 24
#define DEFAULT_DEF_TIMER_JIFFIES 5

static void def_work_fn(struct work_struct *work)
{
	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

static ssize_t show_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int64_t diff;
	unsigned int udiff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	udiff = (unsigned int) diff;

	return snprintf(buf, MAX_LONG_SIZE, "%u\n", udiff);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	rq_info.def_timer_jiffies = msecs_to_jiffies(val);

	rq_info.def_start_time = ktime_to_ns(ktime_get());
	return count;
}

static struct kobj_attribute def_timer_ms_attr =
	__ATTR(def_timer_ms, 0600, show_def_timer_ms,
			store_def_timer_ms);

#ifdef SUPPORT_USER_PERF_OP
static ssize_t show_mpctl(struct kobject *kobj,
               struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", rq_info.mpctl);
}

static ssize_t store_mpctl(struct kobject *kobj,
               struct kobj_attribute *attr, const char *buf, size_t count)
{
	snprintf(rq_info.mpctl, MPCTL_MAX_CMD, "%s", buf);
	sysfs_notify(rq_info.kobj, NULL, "mpctl");
	return count;
}

static struct kobj_attribute mpctl_attr =
	__ATTR(mpctl, S_IWUSR | S_IRUSR, show_mpctl,
			store_mpctl);
#endif

static struct attribute *rq_attrs[] = {
	&def_timer_ms_attr.attr,
#ifdef SUPPORT_USER_PERF_OP
	&mpctl_attr.attr,
#endif
	NULL,
};

static struct attribute_group rq_attr_group = {
	.attrs = rq_attrs,
};

static int init_rq_attribs(void)
{
	int err;

#ifdef SUPPORT_USER_PERF_OP
	rq_info.mpctl[0] = '0';
#endif
	rq_info.attr_group = &rq_attr_group;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats",
			&get_cpu_device(0)->kobj);
	if (!rq_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	return err;
}

static int __init msm_rq_stats_init(void)
{
	int ret;

#ifndef CONFIG_SMP
	/* Bail out if this is not an SMP Target */
	rq_info.init = 0;
	return -EPERM;
#endif

	rq_wq = create_singlethread_workqueue("rq_stats");
	WARN_ON(!rq_wq);
	INIT_WORK(&rq_info.def_timer_work, def_work_fn);
	spin_lock_init(&rq_lock);
	rq_info.def_timer_jiffies = DEFAULT_DEF_TIMER_JIFFIES;
	rq_info.def_timer_last_jiffy = 0;
	ret = init_rq_attribs();

	rq_info.init = 1;

	return ret;
}
late_initcall(msm_rq_stats_init);

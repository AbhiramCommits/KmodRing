// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "kmodring_uapi.h"

extern unsigned int kmodring_get_rate(void);
extern void kmodring_set_rate(unsigned int new_rate);

static ssize_t rate_hz_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", kmodring_get_rate());
}

static ssize_t rate_hz_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    unsigned int val;
    if (kstrtouint(buf, 10, &val))
        return -EINVAL;
    kmodring_set_rate(val);
    return count;
}

static struct kobj_attribute rate_hz_attr = __ATTR(rate_hz, 0644, rate_hz_show, rate_hz_store);

static struct attribute *kmodring_attrs[] = {
    &rate_hz_attr.attr,
    NULL,
};

static const struct attribute_group kmodring_attr_group = {
    .attrs = kmodring_attrs,
};

static struct kobject *kmodring_kobj = NULL;

int kmodring_sysfs_init(void)
{
    kmodring_kobj = kobject_create_and_add("kmodring", kernel_kobj);
    if (!kmodring_kobj)
        return -ENOMEM;

    return sysfs_create_group(kmodring_kobj, &kmodring_attr_group);
}

void kmodring_sysfs_exit(void)
{
    if (kmodring_kobj) {
        kobject_put(kmodring_kobj);
        kmodring_kobj = NULL;
    }
}
MODULE_LICENSE("GPL");

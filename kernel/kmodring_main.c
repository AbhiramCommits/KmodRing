// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/refcount.h>
#include <linux/dma-mapping.h>
#include "kmodring_uapi.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KmodRing Contributors");
MODULE_DESCRIPTION("Lock-free ring buffer char device driver with mmap and kthread producer");
MODULE_VERSION("0.2");

#define DEFAULT_SLOT_COUNT 1024
#define DEFAULT_SLOT_SIZE  256

struct kmodring_ctx {
    refcount_t refcount;
    struct kmodring_hdr *hdr;
    void *ring_mem;
    dma_addr_t dma_handle;
    size_t total_size;
    bool is_dma;
    bool is_vmalloc;
};

static struct kmodring_ctx *global_ctx = NULL;
static DEFINE_MUTEX(kmodring_mutex);

char *alloc_mode = "pages";
module_param(alloc_mode, charp, 0444);
MODULE_PARM_DESC(alloc_mode, "Allocation mode: pages or dma");

extern int kmodring_producer_init(struct kmodring_ctx *ctx);
extern void kmodring_producer_exit(void);
extern wait_queue_head_t *kmodring_get_wq(void);
extern int kmodring_sysfs_init(void);
extern void kmodring_sysfs_exit(void);
extern void *kmodring_dma_alloc(size_t size, dma_addr_t *dma_handle);
extern void kmodring_dma_free(size_t size, void *cpu_addr, dma_addr_t dma_handle);

static vm_fault_t kmodring_vm_fault(struct vm_fault *vmf)
{
    pr_warn("kmodring: out-of-range vm_fault at offset %lu, triggering SIGBUS\n", vmf->pgoff);
    return VM_FAULT_SIGBUS;
}

static const struct vm_operations_struct kmodring_vm_ops = {
    .fault = kmodring_vm_fault,
};

static int kmodring_open(struct inode *inode, struct file *filp)
{
    struct kmodring_ctx *ctx;
    
    mutex_lock(&kmodring_mutex);
    if (!global_ctx) {
        mutex_unlock(&kmodring_mutex);
        return -ENODEV;
    }
    ctx = global_ctx;
    refcount_inc(&ctx->refcount);
    mutex_unlock(&kmodring_mutex);

    filp->private_data = ctx;
    return 0;
}

static int kmodring_release(struct inode *inode, struct file *filp)
{
    struct kmodring_ctx *ctx = filp->private_data;
    if (ctx && refcount_dec_and_test(&ctx->refcount)) {
        // Freed when global teardown happens
    }
    return 0;
}

static ssize_t kmodring_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    struct kmodring_ctx *ctx = filp->private_data;
    struct kmodring_hdr *hdr = ctx->hdr;
    uint64_t tail, head;
    uint32_t slot_count = hdr->slot_count;
    uint32_t slot_size = hdr->slot_size;
    void *slots_base = (void *)hdr + PAGE_SIZE;
    void *slot_ptr;
    struct kmodring_record *rec;

    head = smp_load_acquire(&hdr->producer.head);
    tail = hdr->consumer.tail;

    if (tail == head) {
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(*(kmodring_get_wq()), smp_load_acquire(&hdr->producer.head) != hdr->consumer.tail))
            return -ERESTARTSYS;
        head = smp_load_acquire(&hdr->producer.head);
        tail = hdr->consumer.tail;
    }

    if (len < slot_size)
        return -EINVAL;

    slot_ptr = slots_base + (size_t)((tail % slot_count) * slot_size);
    rec = (struct kmodring_record *)slot_ptr;

    if (copy_to_user(buf, rec, slot_size))
        return -EFAULT;

    smp_store_release(&hdr->consumer.tail, tail + 1);
    return slot_size;
}

static __poll_t kmodring_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct kmodring_ctx *ctx = filp->private_data;
    struct kmodring_hdr *hdr = ctx->hdr;
    __poll_t mask = 0;

    poll_wait(filp, kmodring_get_wq(), wait);

    if (smp_load_acquire(&hdr->producer.head) != hdr->consumer.tail) {
        mask |= EPOLLIN | EPOLLRDNORM;
    }
    return mask;
}

static int kmodring_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct kmodring_ctx *ctx = filp->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size != ctx->total_size)
        return -EINVAL;

    vma->vm_ops = &kmodring_vm_ops;
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

    if (ctx->is_dma) {
        if (dma_mmap_coherent(NULL, vma, ctx->ring_mem, ctx->dma_handle, size))
            return -EAGAIN;
    } else if (ctx->is_vmalloc) {
        struct page *page;
        unsigned long pfn;
        unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
        void *addr = ctx->ring_mem + off;

        if (off >= ctx->total_size)
            return -EINVAL;

        page = vmalloc_to_page(addr);
        pfn = page_to_pfn(page);
        if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
            return -EAGAIN;
    } else {
        if (remap_pfn_range(vma, vma->vm_start, virt_to_pfn(ctx->ring_mem), size, vma->vm_page_prot))
            return -EAGAIN;
    }

    return 0;
}

static long kmodring_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct kmodring_ctx *ctx = filp->private_data;
    struct kmodring_hdr *hdr = ctx->hdr;

    switch (cmd) {
    case KMODRING_IOC_GETINFO: {
        struct kmodring_info info = {
            .slot_size = hdr->slot_size,
            .slot_count = hdr->slot_count,
            .version = hdr->version,
            .flags = 0,
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        break;
    }
    case KMODRING_IOC_SETRATE: {
        unsigned int new_rate;
        if (copy_from_user(&new_rate, (void __user *)arg, sizeof(new_rate)))
            return -EFAULT;
        kmodring_set_rate(new_rate);
        break;
    }
    case KMODRING_IOC_START:
        // Already started by default
        break;
    case KMODRING_IOC_STOP:
        kmodring_set_rate(0);
        break;
    case KMODRING_IOC_STATS: {
        if (copy_to_user((void __user *)arg, hdr, sizeof(struct kmodring_hdr)))
            return -EFAULT;
        break;
    }
    case KMODRING_IOC_INJECT: {
        struct kmodring_record user_rec;
        uint64_t head, tail;
        void *slots_base = (void *)hdr + PAGE_SIZE;
        void *slot_ptr;

        if (copy_from_user(&user_rec, (void __user *)arg, sizeof(user_rec)))
            return -EFAULT;

        head = smp_load_acquire(&hdr->producer.head);
        tail = hdr->consumer.tail;

        if (head - tail >= hdr->slot_count) {
            hdr->drops++;
            return -ENOSPC;
        }

        slot_ptr = slots_base + (size_t)((head % hdr->slot_count) * hdr->slot_size);
        memcpy(slot_ptr, &user_rec, hdr->slot_size);

        smp_store_release(&hdr->producer.head, head + 1);
        hdr->produced++;
        wake_up_interruptible(kmodring_get_wq());
        break;
    }
    case KMODRING_IOC_RESET:
        hdr->producer.head = 0;
        hdr->consumer.tail = 0;
        hdr->drops = 0;
        hdr->produced = 0;
        hdr->wakeups = 0;
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

static const struct file_operations kmodring_fops = {
    .owner = THIS_MODULE,
    .open = kmodring_open,
    .release = kmodring_release,
    .read = kmodring_read,
    .unlocked_ioctl = kmodring_ioctl,
    .poll = kmodring_poll,
    .mmap = kmodring_mmap,
    .llseek = no_llseek,
};

static struct miscdevice kmodring_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "kmodring",
    .fops = &kmodring_fops,
};

static int __init kmodring_init(void)
{
    int ret;
    size_t slot_count = DEFAULT_SLOT_COUNT;
    size_t slot_size = DEFAULT_SLOT_SIZE;
    size_t total_size = PAGE_SIZE + (slot_count * slot_size);

    global_ctx = kzalloc(sizeof(struct kmodring_ctx), GFP_KERNEL);
    if (!global_ctx)
        return -ENOMEM;

    refcount_set(&global_ctx->refcount, 1);
    global_ctx->total_size = total_size;

    if (strcmp(alloc_mode, "dma") == 0) {
        global_ctx->is_dma = true;
        global_ctx->ring_mem = kmodring_dma_alloc(total_size, &global_ctx->dma_handle);
        if (!global_ctx->ring_mem) {
            pr_err("kmodring: failed to allocate dma coherent memory\n");
            ret = -ENOMEM;
            goto err_free_ctx;
        }
        memset(global_ctx->ring_mem, 0, total_size);
    } else {
        global_ctx->is_vmalloc = true;
        global_ctx->ring_mem = vzalloc(total_size);
        if (!global_ctx->ring_mem) {
            ret = -ENOMEM;
            goto err_free_ctx;
        }
    }

    global_ctx->hdr = (struct kmodring_hdr *)global_ctx->ring_mem;
    global_ctx->hdr->magic = KMODRING_MAGIC;
    global_ctx->hdr->version = 1;
    global_ctx->hdr->slot_size = slot_size;
    global_ctx->hdr->slot_count = slot_count;
    global_ctx->hdr->producer.head = 0;
    global_ctx->hdr->consumer.tail = 0;

    ret = kmodring_producer_init(global_ctx);
    if (ret)
        goto err_free_mem;

    ret = kmodring_sysfs_init();
    if (ret)
        goto err_stop_prod;

    ret = misc_register(&kmodring_misc);
    if (ret)
        goto err_sysfs_exit;

    pr_info("kmodring: loaded successfully with alloc_mode=%s\n", alloc_mode);
    return 0;

err_sysfs_exit:
    kmodring_sysfs_exit();
err_stop_prod:
    kmodring_producer_exit();
err_free_mem:
    if (global_ctx->is_dma)
        kmodring_dma_free(total_size, global_ctx->ring_mem, global_ctx->dma_handle);
    else
        vfree(global_ctx->ring_mem);
err_free_ctx:
    kfree(global_ctx);
    global_ctx = NULL;
    return ret;
}

static void __exit kmodring_exit(void)
{
    misc_deregister(&kmodring_misc);
    kmodring_sysfs_exit();
    kmodring_producer_exit();
    if (global_ctx) {
        if (global_ctx->is_dma)
            kmodring_dma_free(global_ctx->total_size, global_ctx->ring_mem, global_ctx->dma_handle);
        else if (global_ctx->ring_mem)
            vfree(global_ctx->ring_mem);
        kfree(global_ctx);
        global_ctx = NULL;
    }
    pr_info("kmodring: unloaded successfully\n");
}

module_init(kmodring_init);
module_exit(kmodring_exit);

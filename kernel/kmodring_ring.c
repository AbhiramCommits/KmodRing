// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "kmodring_uapi.h"

static struct task_struct *producer_kthread = NULL;
static struct hrtimer prod_timer;
static spinlock_t prod_lock;
static wait_queue_head_t prod_wq;
static atomic_t timer_fired_flag = ATOMIC_INIT(0);

static unsigned int rate_hz = 1000;
module_param(rate_hz, uint, 0644);
MODULE_PARM_DESC(rate_hz, "Kernel producer rate in records/sec");

static enum hrtimer_restart prod_timer_callback(struct hrtimer *timer)
{
    atomic_set(&timer_fired_flag, 1);
    wake_up_process(producer_kthread);
    
    if (rate_hz > 0) {
        u64 ns_interval = NSEC_PER_SEC / rate_hz;
        hrtimer_forward_now(timer, ns_to_ktime(ns_interval));
        return HRTIMER_RESTART;
    }
    return HRTIMER_NORESTART;
}

static int producer_thread_fn(void *data)
{
    struct kmodring_ctx *ctx = data;
    struct kmodring_hdr *hdr;
    void *slots_base;
    u64 seq = 0;

    if (!ctx)
        return 0;

    hdr = ctx->hdr;
    slots_base = (void *)hdr + PAGE_SIZE;

    while (!kthread_should_stop()) {
        set_current_state(TASK_INTERRUPTIBLE);
        if (!atomic_read(&timer_fired_flag) && !kthread_should_stop())
            schedule();
        __set_current_state(TASK_RUNNING);

        atomic_set(&timer_fired_flag, 0);

        if (kthread_should_stop())
            break;

        spin_lock_irq(&prod_lock);
        {
            u64 head = smp_load_acquire(&hdr->producer.head);
            u64 tail = hdr->consumer.tail;

            if (head - tail >= hdr->slot_count) {
                hdr->drops++;
            } else {
                void *slot_ptr = slots_base + (size_t)((head % hdr->slot_count) * hdr->slot_size);
                struct kmodring_record *rec = (struct kmodring_record *)slot_ptr;

                rec->seq = ++seq;
                rec->tsc_or_ktime_ns = ktime_get_ns();
                rec->len = hdr->slot_size - sizeof(struct kmodring_record);
                rec->flags = 0;

                smp_store_release(&hdr->producer.head, head + 1);
                hdr->produced++;
                hdr->wakeups++;
                wake_up_interruptible(&prod_wq);
            }
        }
        spin_unlock_irq(&prod_lock);
    }
    return 0;
}

int kmodring_producer_init(struct kmodring_ctx *ctx)
{
    spin_lock_init(&prod_lock);
    init_waitqueue_head(&prod_wq);

    producer_kthread = kthread_run(producer_thread_fn, ctx, "kmodring_prod");
    if (IS_ERR(producer_kthread))
        return PTR_ERR(producer_kthread);

    hrtimer_init(&prod_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    prod_timer.function = prod_timer_callback;
    if (rate_hz > 0) {
        u64 ns_interval = NSEC_PER_SEC / rate_hz;
        hrtimer_start(&prod_timer, ns_to_ktime(ns_interval), HRTIMER_MODE_REL);
    }

    return 0;
}

void kmodring_producer_exit(void)
{
    hrtimer_cancel(&prod_timer);
    if (producer_kthread) {
        kthread_stop(producer_kthread);
        producer_kthread = NULL;
    }
}

wait_queue_head_t *kmodring_get_wq(void)
{
    return &prod_wq;
}

unsigned int kmodring_get_rate(void)
{
    return rate_hz;
}

void kmodring_set_rate(unsigned int new_rate)
{
    rate_hz = new_rate;
    if (rate_hz > 0) {
        u64 ns_interval = NSEC_PER_SEC / rate_hz;
        hrtimer_cancel(&prod_timer);
        hrtimer_start(&prod_timer, ns_to_ktime(ns_interval), HRTIMER_MODE_REL);
    } else {
        hrtimer_cancel(&prod_timer);
    }
}
MODULE_LICENSE("GPL");

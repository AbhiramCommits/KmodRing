/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _UAPI_KMODRING_UAPI_H
#define _UAPI_KMODRING_UAPI_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define KMODRING_MAGIC 'k'

struct kmodring_hdr {
    uint32_t magic;
    uint32_t version;
    uint32_t slot_size;
    uint32_t slot_count;
    uint32_t _pad0;
    uint32_t _pad1;

    /* Cacheline isolated producer head and consumer tail */
    struct {
        uint64_t head;
        uint8_t _pad_cache0[56];
    } __attribute__((aligned(64))) producer;

    struct {
        uint64_t tail;
        uint8_t _pad_cache1[56];
    } __attribute__((aligned(64))) consumer;

    uint64_t drops;
    uint64_t produced;
    uint64_t wakeups;
} __attribute__((aligned(64)));

struct kmodring_record {
    uint64_t seq;
    uint64_t tsc_or_ktime_ns;
    uint32_t len;
    uint32_t flags;
    uint8_t payload[];
};

#define KMODRING_IOC_MAGIC 'k'
#define KMODRING_IOC_GETINFO  _IOR(KMODRING_IOC_MAGIC, 1, struct kmodring_info)
#define KMODRING_IOC_SETRATE  _IOW(KMODRING_IOC_MAGIC, 2, uint32_t)
#define KMODRING_IOC_START    _IO(KMODRING_IOC_MAGIC,  3)
#define KMODRING_IOC_STOP     _IO(KMODRING_IOC_MAGIC,  4)
#define KMODRING_IOC_INJECT   _IOW(KMODRING_IOC_MAGIC, 5, struct kmodring_record)
#define KMODRING_IOC_STATS    _IOR(KMODRING_IOC_MAGIC, 6, struct kmodring_hdr)
#define KMODRING_IOC_RESET    _IO(KMODRING_IOC_MAGIC,  7)

struct kmodring_info {
    uint32_t slot_size;
    uint32_t slot_count;
    uint32_t version;
    uint32_t flags;
};

#endif /* _UAPI_KMODRING_UAPI_H */

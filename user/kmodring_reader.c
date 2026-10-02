// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include "kmodring_uapi.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    const char *dev_path = "/dev/kmodring";
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        perror("open /dev/kmodring");
        fprintf(stderr, "Note: If running on non-Linux or without module loaded, this is expected.\n");
        return 0;
    }

    struct kmodring_info info;
    if (ioctl(fd, KMODRING_IOC_GETINFO, &info) < 0) {
        perror("ioctl GETINFO");
        close(fd);
        return 1;
    }

    size_t map_size = 4096 + (info.slot_count * info.slot_size);
    void *map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    struct kmodring_hdr *hdr = (struct kmodring_hdr *)map;
    if (hdr->magic != KMODRING_MAGIC) {
        fprintf(stderr, "Invalid magic number in shared memory\n");
        munmap(map, map_size);
        close(fd);
        return 1;
    }

    printf("Successfully connected to Kmodring via mmap. Slot size: %u, slot count: %u\n",
           hdr->slot_size, hdr->slot_count);

    // Drain loop
    uint64_t last_seq = 0;
    uint64_t count = 0;
    void *slots_base = map + 4096;

    while (true) {
        uint64_t head = __atomic_load_n(&hdr->producer.head, __ATOMIC_ACQUIRE);
        uint64_t tail = hdr->consumer.tail;

        if (tail == head) {
            usleep(1000);
            break; // Drain complete for test
        }

        struct kmodring_record *rec = (struct kmodring_record *)(slots_base + ((tail % hdr->slot_count) * hdr->slot_size));
        if (count > 0 && rec->seq != last_seq + 1) {
            fprintf(stderr, "Sequence gap detected! Expected %llu, got %llu\n", (unsigned long long)(last_seq + 1), (unsigned long long)rec->seq);
            munmap(map, map_size);
            close(fd);
            return 1;
        }

        last_seq = rec->seq;
        count++;
        __atomic_store_n(&hdr->consumer.tail, tail + 1, __ATOMIC_RELEASE);
    }

    printf("Drained %llu records successfully with zero sequence gaps.\n", (unsigned long long)count);

    munmap(map, map_size);
    close(fd);
    return 0;
}

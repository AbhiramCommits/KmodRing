// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include "kmodring_uapi.h"

static struct platform_device *kmodring_pdev = NULL;

void *kmodring_dma_alloc(size_t size, dma_addr_t *dma_handle)
{
    kmodring_pdev = platform_device_register_simple("kmodring_dma", -1, NULL, 0);
    if (IS_ERR(kmodring_pdev)) {
        kmodring_pdev = NULL;
        return NULL;
    }

    return dma_alloc_coherent(&kmodring_pdev->dev, size, dma_handle, GFP_KERNEL);
}

void kmodring_dma_free(size_t size, void *cpu_addr, dma_addr_t dma_handle)
{
    if (kmodring_pdev && cpu_addr) {
        dma_free_coherent(&kmodring_pdev->dev, size, cpu_addr, dma_handle);
        platform_device_unregister(kmodring_pdev);
        kmodring_pdev = NULL;
    }
}
MODULE_LICENSE("GPL");

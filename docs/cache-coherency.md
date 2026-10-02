# KmodRing Cache Coherency & Architecture

## Cache Coherency on x86_64 vs ARM/aarch64

### x86_64
x86_64 processors feature hardware-enforced cache coherency with strong memory ordering (Total Store Order / TSO). 
- CPU caches are fully coherent across cores.
- `dma_alloc_coherent` returns regular memory mapped as cacheable, and standard CPU stores are immediately visible to other cores and devices without explicit cache flushing.
- `smp_store_release()` and `smp_load_acquire()` (or compiler acquire/release atomics) suffice to prevent compiler reordering and ensure multi-core visibility.

### ARM / aarch64
ARM architectures are weakly ordered and frequently feature non-coherent DMA hardware paths.
- Non-coherent devices require explicit cache maintenance operations (`dma_sync_single_for_cpu` and `dma_sync_single_for_device`) or Write-Combining / uncached memory mappings (`pgprot_writecombine` or `pgprot_noncached`) to prevent stale CPU cache lines from shadowing device updates or vice-versa.
- `dma_alloc_coherent` automatically allocates memory that is coherent for DMA operations, but explicit memory barriers (`dmb`, `dsb`) or acquire/release semantics are required for ring pointer synchronization between the kernel producer and userspace consumer.

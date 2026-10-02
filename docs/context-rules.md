# KmodRing Context Rules & Lock Ordering

## Execution Contexts & Synchronization

| Component / Path | Execution Context | May Sleep? | Lock Acquired | Lock Ordering Rule |
| :--- | :--- | :--- | :--- | :--- |
| `file_operations .open / .release` | Process context | Yes | `kmodring_mutex` | Acquire `kmodring_mutex` before modifying `global_ctx` refcount. Never acquire spinlocks while holding this mutex. |
| `file_operations .read / .mmap` | Process context | Yes | None (uses atomic loads/stores and user copy) | N/A |
| `file_operations .unlocked_ioctl` | Process context | Yes | None (or rate setter updates) | N/A |
| `hrtimer callback` (`prod_timer_callback`) | Softirq / Atomic context | **NO** | `spin_lock_irqsave(&prod_lock)` | Must be non-blocking. Never sleep or allocate memory without GFP_ATOMIC. |
| `kthread producer` (`producer_thread_fn`) | Process context / kthread | Yes | `spin_lock_irq` around ring push | Acquire spinlock with IRQ disabled only when pushing to ring. |

## Overflow Policy
When the ring buffer is full (`head - tail >= slot_count`), the producer drops the newest record, increments `hdr->drops`, and never blocks in atomic/softirq context.

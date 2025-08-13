# Lab 5: Thread and User Process Implementation

This document details the implementation of multitasking, user processes, system calls, and POSIX signals for an operating system. The project successfully implements kernel-level threads, user-level processes with preemption via timer interrupts, and a full POSIX signal handling mechanism.

---

## Basic Exercise 1 - Thread

This exercise involved implementing the core components for kernel-level multithreading, including thread creation, scheduling, context switching, and termination.

### Implementation Details

* **Thread Structure (`thread.h`):**
    * A central `thread_t` structure was defined to act as the Thread Control Block (TCB). It holds the thread's ID, state (`THREAD_RUNNING`, `THREAD_READY`, `THREAD_DEAD`), stack pointers, and signal handling information.
    * The context for a context switch is stored in `thread_context_block_t`, which saves all callee-saved registers (`x19`-`x28`), the frame pointer (`fp`), and the link register (`lr`).
    * Threads are managed in a global circular linked list which serves as the **run queue**. The head of this queue is `run_queue`.

* **Thread Creation (`thread.c`):**
    * The `thread_create()` function allocates memory for a new `thread_t` struct and a kernel stack (`THREAD_STACK_SIZE`).
    * It initializes the thread's context using the `thread_create_save()` assembly function. This function sets the new thread's link register (`lr`) to its entry point function and prepares its stack pointer (`sp`), making it ready for scheduling.
    * The new thread is then added to the run queue using `add_to_run_queue()`.

* **Scheduler and Context Switch (`thread.c`, `switch_thread.S`):**
    * The scheduler logic is implemented in `schedule()`. It performs a **round-robin** scheduling by simply selecting the next thread in the run queue.
    * The actual context switch is handled by the assembly function `switch_to(prev_tcb, next_tcb)`. It meticulously saves the callee-saved registers of the `prev` thread into its TCB and loads the registers from the `next` thread's TCB.
    * The current running thread's TCB pointer is stored in and retrieved from the `tpidr_el1` system register. The `get_current()` assembly function provides a convenient way to access this pointer from C code.

* **Thread Termination and Idle Thread (`thread.c`, `process.c`):**
    * When a thread finishes, it calls `thread_exit()`, which marks its state as `THREAD_DEAD` and removes it from the run queue.
    * An `idle()` thread is implemented. Its main loop continuously calls `kill_zombie_thread()` to reclaim the memory (stack and TCB) of any `THREAD_DEAD` threads.
    * After cleanup, the `idle()` thread calls `schedule()` to yield the CPU to any other runnable thread, ensuring the system never halts.

---

## Basic Exercise 2 - User Process and System Call

This section covers the creation of user-level processes that are isolated from the kernel, communicating only through a well-defined system call interface.
> [!WARNING]  
> Basic Exercise 2 and after exercise are different from the Basic Exercise 1, please consider as independent project 

### Implementation Details

* **Trap Frame (`syscall.h`):**
    * A `trap_frame_t` struct was defined to save the complete context of a user process when it traps into the kernel. This includes all general-purpose registers (`x0-x30`), the user stack pointer (`sp_el0`), and the exception state registers (`spsr_el1`, `elr_el1`).

* **Exception Handling (`exception.S`, `exception_entry.c`):**
    * When a user process executes an `svc` instruction, the CPU switches to EL1 and jumps to `sync_lower_el_64_handler`.
    * The `svc_save_all` macro saves the entire user context onto the kernel stack, creating the `trap_frame`.
    * Control is then passed to the C function `sync_lower_el_64_entry`, which identifies the event as a syscall and calls `handle_syscall(frame_ptr)`.
    * After the syscall is processed, `svc_load_all` restores the (potentially modified) user context from the trap frame, and `eret` returns control back to the user process.

* **System Call Implementation (`syscall.c`):**
    * A central `handle_syscall` function acts as a dispatcher. It reads the system call number from register `x8` (which is saved in the trap frame) and calls the appropriate implementation function.
    * The following system calls were implemented as required:
        * `sys_getpid()`: Returns the current thread's ID.
        * `sys_uart_read()` & `sys_uart_write()`: Handle character I/O.
        * `sys_exec()`: Loads a new user program from the `initramfs` CPIO archive, overwrites the current process's memory space, and sets up the trap frame to start execution at the new program's entry point.
        * `sys_fork()`: Creates a new child process. This is achieved by:
            1.  Creating a new thread and kernel stack for the child.
            2.  Allocating a new user memory space and copying the parent's entire user space memory to the child's.
            3.  Copying the parent's `trap_frame` to the child's TCB.
            4.  Modifying the return value (`x0`) in the respective trap frames: the child's `x0` is set to `0`, and the parent's `x0` is set to the child's PID.
        * `sys_exit()`: Terminates the current process by marking its state as `THREAD_DEAD` and invoking the scheduler.
        * `sys_mbox_call()`: Allows user processes to communicate with hardware via the VideoCore mailbox.
        * `sys_kill()`: A basic version that simply terminates a process by its PID. This is later enhanced by the POSIX signal implementation.

---

## Video Player (User Preemption)

To test the full system, user preemption was enabled using timer interrupts. This allows the kernel to forcibly switch between user processes, creating a smooth multitasking experience, as demonstrated by the video player test program.

### Implementation Details

* **Timer Initialization (`run_userprogram.c`):**
    * The `el0_core_timer_enable()` function is called before switching to a user process for the first time.
    * It configures the ARM core timer to be accessible from EL0 by setting the `CNTKCTL_EL1.EL0PCTEN` bit.
    * It enables the timer and sets an initial timeout value (calculated from the CPU frequency `cntfrq_el0`), and unmasks the timer interrupt in the interrupt controller.

* **Preemptive Scheduling (`timeout.c`, `process.c`):**
    * When the timer expires, an IRQ is triggered from EL0. The CPU traps to `lower_el_irq_exception_handler`.
    * The handler saves the user context into a `trap_frame` and calls `lower_el_irq_entry`, which identifies the interrupt source.
    * For a core timer interrupt, it calls `user_timeout_handler()`.
    * This handler is the core of preemption: it calls `user_thread_schedule(frame)` to perform a context switch to the next available user process. The saving and restoring of the full `trap_frame` ensures a seamless switch.
    * Finally, it resets the timer for the next time slice before returning from the exception. This ensures periodic scheduling.

---

## Advanced Exercise 1 - POSIX Signal

A complete POSIX-style signal handling mechanism was implemented, allowing for asynchronous inter-process communication and custom signal handlers running in user mode.

### Implementation Details

The implementation follows a precise flow to safely execute a user-defined handler:

1.  **Data Structures (`thread.h`):**
    * The `thread_t` struct was extended with:
        * `sigpending`: A 32-bit integer acting as a bitmask for pending signals.
        * `sighand[NSIG]`: An array of function pointers to store the registered handler for each signal.
        * `signal_backup_frame`: A `trap_frame_t` to store the original process context before jumping to a signal handler.
        * `is_handling_signal`: A flag to prevent nested signal handling.
        * `handler_stack_ptr`: A pointer to the temporary stack allocated for the user-mode handler.

2.  **Registration and Sending (`syscall.c`):**
    * A new `sys_signal(signal, handler)` syscall was added (syscall #8). It registers `handler` for the given `signal` number in the current process's `sighand` array.
    * A `sys_kill_signal(pid, signal)` syscall was added (syscall #9). It finds the target process by `pid` and sets the appropriate bit in its `sigpending` mask.

3.  **Signal Delivery (`signal.c`):**
    * Upon any exception (syscall or interrupt), just before returning to EL0, the kernel calls `check_signals(frame)`.
    * This function checks the `sigpending` bitmask. If a pending signal is found:
        * **Default Handler:** If the handler is `SIG_DFL` (e.g., for `SIGKILL`), the kernel performs the default action, such as terminating the process via `user_thread_exit()`.
        * **User-Defined Handler:** This is the complex case:
            a. The current user context (the `trap_frame`) is backed up into `current->signal_backup_frame`.
            b. A new stack is allocated for the handler using `dynamic_malloc()`.
            c. The *current* trap frame is modified to divert execution:
                * `elr_el1` (return address) is set to the user's registered handler function.
                * `sp_el0` (stack pointer) is set to the top of the newly allocated handler stack.
                * `x0` (first argument) is set to the signal number.
                * **`x30` (link register)** is set to the address of a special kernel trampoline, `sigreturn_trampoline_entry`.

4.  **Returning from Handler (`switch_thread.S`, `syscall.c`):**
    * When the user's signal handler function finishes, it executes a `ret` instruction, which jumps to the address in `x30`—our `sigreturn_trampoline_entry`.
    * The trampoline immediately executes `svc #0` with syscall number 10 (`SYS_SIGRETURN`).
    * The `sys_sigreturn()` handler then performs the cleanup:
        a. It restores the original user context from `current->signal_backup_frame` back into the main trap frame.
        b. It frees the temporary handler stack.
        c. It clears the `is_handling_signal` flag.
    * The kernel then returns from the exception using `eret`, and the process resumes its original execution as if it were never interrupted.

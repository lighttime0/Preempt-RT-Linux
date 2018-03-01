# A realtime preemption overview

> 原文地址：[https://lwn.net/Articles/146861/](https://lwn.net/Articles/146861/)

```
August 10, 2005
This article was contributed by Paul McKenney
```

有很多papers

There have been a considerable number of papers describing a number of different aspects of and approaches to realtime, a few of which were listed in the RESOURCES section of [my "realtime patch acceptance summary](https://lwn.net/Articles/143323/) from July.

However, there does not appear to be a similar description of the realtime preemption (PREEMPT_RT) patch. This document attempts to fill this gap, using the V0.7.52-16 version of this patch. However, please note that the PREEMPT_RT patch evolves very quickly!

## Philosophy of PREEMPT_RT

PREEMPT_RT patch的核心思路是将kernel代码中不可抢占的代码量最小化，同时将需要修改的代码量最小化。临界区代码、中断handlers、interrupt-disable code sequences通常是可以抢占的。

The key point of the PREEMPT_RT patch is to minimize the amount of kernel code that is non-preemptible, while also minimizing the amount of code that must be changed in order to provide this added preemptibility. In particular, critical sections, interrupt handlers, and interrupt-disable code sequences are normally preemptible. The PREEMPT_RT patch leverages the SMP capabilities of the Linux kernel to add this extra preemptibility without requiring a complete kernel rewrite. In a sense, one can loosely think of a preemption as the addition of a new CPU to the system, and then use the normal locking primitives to synchronize with any action taken by the preempting task.

Note that this statement of philosophy should not be taken too literally, for example, the PREEMPT_RT patch does not actually perform a CPU hot-plug event for each preemption. Instead, the point is that the underlying mechanisms used to tolerate (almost) unlimited preemption are those that must be provided for SMP environments. More information on how this philosophy is applied is given in the following sections.

## Features of PREEMPT_RT

这一部分将对PREEMPT_RT patch提供的特性做一个总览。

<!-- This section gives an overview of the features that the PREEMPT_RT patch provides. -->

1. Preemptible critical sections
2. Preemptible interrupt handlers
3. Preemptible "interrupt disable" code sequences
4. Priority inheritance for in-kernel spinlocks and semaphores
5. Deferred operations
6. Latency-reduction measures

上面的每个topic都会在下面的小节中涉及到。

<!-- Each of these topics is covered in the following sections. -->

### Preemptible critical sections

在PREEMPT中，大部分的spinlock（包括`spinlock_t`和`rwlock_t`）都是可抢占的，RCU的读者临界区（`rcu_read_lock()`和`rcu_read_unlock()`中的代码）。信号量的临界区是可抢占的，但是在PREEMPT和non-PREEMPT的kernels中，它都是可抢占的（后面会有更多关于信号量的解释）。抢占意味着你在获取一个spinlock时可以阻塞，也就是说，不允许在获取spinlock时禁用抢占或者中断（这个规则有一个例外是`_trylock`变量，至少在你不在一个紧凑的循环里重复地调用它时）。这也意味着，使用`spinlock_t`时，`spin_lock_irqsave()`并不禁用硬件中断。

<!-- In PREEMPT_RT, normal spinlocks (spinlock_t and rwlock_t) are preemptible, as are RCU read-side critical sections (rcu_read_lock() and rcu_read_unlock()). Semaphore critical sections are preemptible, but they already are in both PREEMPT and non-PREEMPT kernels (but more on semaphores later). This preemptibility means that you can block while acquiring a spinlock, which in turn means that it is illegal to acquire a spinlock with either preemption or interrupts disabled (the one exception to this rule being the _trylock variants, at least as long as you don't repeatedly invoke them in a tight loop). This also means that spin_lock_irqsave() does -not- disable hardware interrupts when used on a spinlock_t. -->

**Quick Quiz #1**：在non-preemptible kernel中，信号量的临界区是如何被抢占的？

<!-- **Quick Quiz #1**: How can semaphore critical sections be preempted in a non-preemptible kernel? -->

这样一来，如果你需要一个既不允许中断，也不允许抢占的锁，该怎么办呢？你需要使用`raw_spinlock_t`来代替`spinlock_t`，但是对`raw_spinlock_t`仍然使用`spin_lock()`系列的函数。PREEMPT_RT patch包含了一些列的宏，使得`spin_lock()`的行为类似于C++语言的重载函数——当调用在`raw_spinlock_t`上时，它的行为类似于传统的spinlock；但是当调用在`spinlock_t`上时，它的临界区可以被抢占。举例来说，各种各样的`_irq`原语（比如`spin_lock_irqsave()`）会在申请`raw_spinlock_t`时禁用硬件中断，但是在申请`spinlock_t`时不会禁用硬件中断。

批注：原语 操作系统或计算机网络用语范畴。是由若干条指令组成的，用于完成一定功能的一个过程。primitive or atomic action 是由若干个机器指令构成的完成某种特定功能的一段程序，具有不可分割性。即原语的执行必须是连续的，在执行过程中不允许被中断。

So, what to do if you need to acquire a lock when either interrupts or preemption are disabled? You use a raw_spinlock_t instead of a spinlock_t, but continue invoking spin_lock() and friends on the raw_spinlock_t. The PREEMPT_RT patch includes a set of macros that cause spin_lock() to act like a C++ overloaded function -- when invoked on a raw_spinlock_t, it acts like a traditional spinlock, but when invoked on a spinlock_t, its critical section can be preempted. For example, the various _irq primitives (e.g., spin_lock_irqsave()) disable hardware interrupts when applied to a raw_spinlock_t, but do not when applied to a spinlock_t. However, use of raw_spinlock_t (and its rwlock_t counterpart, raw_rwlock_t) should be the exception, not the rule. These raw locks should not be needed outside of a few low-level areas, such as the scheduler, architecture-specific code, and RCU.

Since critical sections can now be preempted, you cannot rely on a given critical section executing on a single CPU -- it might move to a different CPU due to being preempted. So, when you are using per-CPU variables in a critical section, you must separately handle the possibility of preemption, since spinlock_t and rwlock_t are no longer doing that job for you. Approaches include:

Explicitly disable preemption, either through use of get_cpu_var(), preempt_disable(), or disabling hardware interrupts.
Use a per-CPU lock to guard the per-CPU variables. One way to do this is by using the new DEFINE_PER_CPU_LOCKED() primitive -- more on this later.
Since spin_lock() can now sleep, an additional task state was added. Consider the following code sequence (supplied by Ingo Molnar):

	spin_lock(&mylock1);
	current->state = TASK_UNINTERRUPTIBLE;
	spin_lock(&mylock2);                    // [*]
	blah();
	spin_unlock(&mylock2);
	spin_unlock(&mylock1);
Since the second spin_lock() call can sleep, it can clobber the value of current->state, which might come as quite a surprise to the blah() function. The new TASK_RUNNING_MUTEX bit is used to allow the scheduler to preserve the prior value of current->state in this case.

Although the resulting environment can be a bit unfamiliar, but it permits critical sections to be preempted with minimal code changes, and allows the same code to work in the PREEMPT_RT, PREEMPT, and non-PREEMPT configurations.

Preemptible interrupt handlers
Almost all interrupt handlers run in process context in the PREEMPT_RT environment. Although any interrupt can be marked SA_NODELAY to cause it to run in interrupt context, only the fpu_irq, irq0, irq2, and lpptest interrupts have SA_NODELAY specified. Of these, only irq0 (the per-CPU timer interrupt) is normally used -- fpu_irq is for floating-point co-processor interrupts, and lpptest is used for interrupt-latency benchmarking. Note that software timers (add_timer() and friends) do not run in hardware interrupt context; instead, they run in process context and are fully preemptible.

Note that SA_NODELAY is not to be used lightly, as can greatly degrade both interrupt and scheduling latencies. The per-CPU timer interrupt qualifies due to its tight tie to scheduling and other core kernel components. Furthermore, SA_NODELAY interrupt handlers must be coded very carefully as noted in the following paragraphs, otherwise, you will see oopses and deadlocks.

Since the per-CPU timer interrupt (e.g., scheduler_tick()) runs in hardware-interrupt context, any locks shared with process-context code must be raw spinlocks (raw_spinlock_t or raw_rwlock_t), and, when acquired from process context, the _irq variants must be used, for example, spin_lock_irqsave(). In addition, hardware interrupts must typically be disabled when process-context code accesses per-CPU variables that are shared with the SA_NODELAY interrupt handler, as described in the following section.

Preemptible "interrupt disable" code sequences
The concept of preemptible interrupt-disable code sequences may seem to be a contradiction in terms, but it is important to keep in mind the PREEMPT_RT philosophy. This philosophy relies on the SMP capabilities of the Linux kernel to handle races with interrupt handlers, keeping in mind that most interrupt handlers run in process context. Any code that interacts with an interrupt handler must be prepared to deal with that interrupt handler running concurrently on some other CPU.

Therefore, spin_lock_irqsave() and related primitives need not disable preemption. The reason this is safe is that if the interrupt handler runs, even if it preempts the code holding the spinlock_t, it will block as soon as it attempts to acquire that spinlock_t. The critical section will therefore still be preserved.

However, local_irq_save() still disables preemption, since there is no corresponding lock to rely on. Using locks instead of local_irq_save() therefore can help reduce scheduling latency, but substituting locks in this manner can reduce SMP performance, so be careful.

Code that must interact with SA_NODELAY interrupts cannot use local_irq_save(), since this does not disable hardware interrupts. Instead, raw_local_irq_save() should be used. Similarly, raw spinlocks (raw_spinlock_t, raw_rwlock_t, and raw_seqlock_t) need to be used when interacting with SA_NODELAY interrupt handlers. However, raw spinlocks and raw interrupt disabling should -not- be used outside of a few low-level areas, such as the scheduler, architecture-dependent code, and RCU.

Priority inheritance for in-kernel spinlocks and semaphores
Realtime programmers are often concerned about priority inversion, which can happen as follows:

Low-priority task A acquires a resource, for example, a lock.
Medium-priority task B starts executing CPU-bound, preempting low-priority task A.
High-priority task C attempts to acquire the lock held by low-priority task A, but blocks because of medium-priority task B having preempted low-priority task A.
Such priority inversion can indefinitely delay a high-priority task. There are two main ways to address this problem: (1) suppressing preemption and (2) priority inheritance. In the first case, since there is no preemption, task B cannot preempt task A, preventing priority inversion from occurring. This approach is used by PREEMPT kernels for spinlocks, but not for semaphores. It does not make sense to suppress preemption for semaphores, since it is legal to block while holding one, which could result in priority inversion even in absence of preemption. For some realtime workloads, preemption cannot be suppressed even for spinlocks, due to the impact to scheduling latencies.

Priority inheritance can be used in cases where suppressing preemption does not make sense. The idea here is that high-priority tasks temporarily donate their high priority to lower-priority tasks that are holding critical locks. This priority inheritance is transitive: in the example above, if an even higher priority task D attempted to acquire a second lock that high-priority task C was already holding, then both tasks C and A would be be temporarily boosted to the priority of task D. The duration of the priority boost is also sharply limited: as soon as low-priority task A releases the lock, it will immediately lose its temporarily boosted priority, handing the lock to (and being preempted by) task C.

However, it may take some time for task C to run, and it is quite possible that another higher-priority task E will try to acquire the lock in the meantime. If this happens, task E will "steal" the lock from task C, which is legal because task C has not yet run, and has therefore not actually acquired the lock. On the other hand, if task C gets to run before task E tries to acquire the lock, then task E will be unable to "steal" the lock, and must instead wait for task C to release it, possibly boosting task C's priority in order to expedite matters.

In addition, there are some cases where locks are held for extended periods. A number of these have been modified to add "preemption points" so that the lock holder will drop the lock if some other task needs it. The JBD journaling layer contains a couple of examples of this.

It turns out that write-to-reader priority inheritance is particularly problematic, so PREEMPT_RT simplifies the problem by permitting only one task at a time to read-hold a reader-writer lock or semaphore, though that task is permitted to recursively acquire it. This makes priority inheritance doable, though it can limit scalability.

Quick Quiz #2: What is a simple and fast way to implement priority inheritance from writers to multiple readers?
In addition, there are some cases where priority inheritance is undesirable for semaphores, for example, when the semaphore is being used as an event mechanism rather than as a lock (you can't tell who will post the event before the fact, and therefore have no idea which task to priority-boost). There are compat_semaphore and compat_rw_semaphore variants that may be used in this case. The various semaphore primitives (up(), down(), and friends) may be used on either compat_semaphore and semaphore, and, similarly, the reader-writer semaphore primitives (up_read(), down_write(), and friends) may be used on either compat_rw_semaphore and rw_semaphore. Often, however, the completion mechanism is a better tool for this job.

So, to sum up, priority inheritance prevents priority inversion, allowing high-priority tasks to acquire locks and semaphores in a timely manner, even if the locks and semaphores are being held by low-priority tasks. PREEMPT_RT's priority inheritance provides transitivity, timely removal of inheritance, and the flexibility required to handle cases when high priority tasks suddenly need locks earmarked for low-priority tasks. The compat_semaphore and compat_rw_semaphore declarations can be used to avoid priority inheritance for semaphores for event-style usage.

Deferred operations
Since spin_lock() can now sleep, it is no longer legal to invoke it while preemption (or interrupts) are disabled. In some cases, this has been solved by deferring the operation requiring the spin_lock() until preemption has been re-enabled:

put_task_struct_delayed() queues up a put_task_struct() to be executed at a later time when it is legal to acquire (for example) the spinlock_t alloc_lock in task_struct.
mmdrop_delayed() queues up an mmdrop() to be executed at a later time, similar to put_task_struct_delayed() above.
TIF_NEED_RESCHED_DELAYED does a reschedule, but waits to do so until the process is ready to return to user space -- or until the next preempt_check_resched_delayed(), whichever comes first. Either way, the point is avoid needless preemptions in cases where a high-priority task being awakened cannot make progress until the current task drops a lock. Without TIF_NEED_RESCHED_DELAYED, the high-priority task would immediately preempt the low-priority task, only to quickly block waiting for the lock held by the low-priority task.
The solution is to change a "wake_up()" that is immediately followed by a spin_unlock() to instead be a "wake_up_process_sync()". If the process being awakened would preempt the current process, the wakeup is delayed via the TIF_NEED_RESCHED_DELAYED flag.

In all of these situations, the solution is to defer an action until that action may be more safely or conveniently performed.

Latency-reduction measures
There are a few changes in PREEMPT_RT whose primary purpose is to reduce scheduling or interrupt latency.

The first such change involves the x86 MMX/SSE hardware. This hardware is handled in the kernel with preemption disabled, and this sometimes means waiting until preceding MMX/SSE instructions complete. Some MMX/SSE instructions are no problem, but others take overly long amounts of time, so PREEMPT_RT refuses to use the slow ones.

The second change applies per-CPU variables to the slab allocator, as an alternative to the previous wanton disabling of interrupts.

Summary of PREEMPT_RT primitives
This section gives a brief list of primitives that are either added by PREEMPT_RT or whose behavior is significantly changed by PREEMPT_RT.

Locking Primitives
spinlock_t
Critical sections are preemptible. The _irq operations (e.g., spin_lock_irqsave()) do -not- disable hardware interrupts. Priority inheritance is used to prevent priority inversion. An underlying rt_mutex is used to implement spinlock_t in PREEMPT_RT (as well as to implement rwlock_t, struct semaphore, and struct rw_semaphore).
raw_spinlock_t
Special variant of spinlock_t that offers the traditional behavior, so that critical sections are non-preemptible and _irq operations really disable hardware interrupts. Note that you should use the normal primitives (e.g., spin_lock()) on raw_spinlock_t. That said, you shouldn't be using raw_spinlock_t -at- -all- except deep within architecture-specific code or low-level scheduling and synchronization primitives. Misuse of raw_spinlock_t will destroy the realtime aspects of PREEMPT_RT. You have been warned.
rwlock_t
Critical sections are preemptible. The _irq operations (e.g., write_lock_irqsave()) do -not- disable hardware interrupts. Priority inheritance is used to prevent priority inversion. In order to keep the complexity of priority inheritance down to a dull roar, only one task may read-acquire a given rwlock_t at a time, though that task may recursively read-acquire the lock.
RW_LOCK_UNLOCKED(mylock)
The RW_LOCK_UNLOCKED macro now takes the lock itself as an argument, which is required for priority inheritance. Unfortunately, this makes its use incompatible with the PREEMPT and non-PREEMPT kernels. Uses of RW_LOCK_UNLOCKED should therefore be changed to DEFINE_RWLOCK().
raw_rwlock_t
Special variant of rwlock_t that offers the traditional behavior, so that critical sections are non-preemptible and _irq operations really disable hardware interrupts. Note that you should use the normal primitives (e.g., read_lock()) on raw_rwlock_t. That said, as with raw_spinlock_t, you shouldn't be using raw_rwlock_t -at- -all- except deep within architecture-specific code or low-level scheduling and synchronization primitives. Misuse of raw_rwlock_t will destroy the realtime aspects of PREEMPT_RT.	You have once again been warned.
seqlock_t
Critical sections are preemptible. Priority inheritance has been applied to the update side (the read-side cannot be involved in priority inversion, since seqlock_t readers do not block writers).
SEQLOCK_UNLOCKED(name)
The SEQLOCK_UNLOCKED macro now takes the lock itself as an argument, which is required for priority inheritance. Unfortunately, this makes its use incompatible with the PREEMPT and non-PREEMPT kernels. Uses of SEQLOCK_UNLOCKED should therefore be changed to use DECLARE_SEQLOCK(). Note that DECLARE_SEQLOCK() defines the seqlock_t and initializes it.
struct semaphore
The struct semaphore is now subject to priority inheritance.
down_trylock()
This primitive can schedule, so cannot be invoked with hardware interrupts disabled or with preemption disabled. However, since almost all interrupts run in process context with both preemption and interrupts enabled, this restriction has no effect thus far.
struct compat_semaphore
A variant of struct semaphore that is -not- subject to priority inheritance. This is useful for cases when you need an event mechanism, rather than a sleeplock.
struct rw_semaphore
The struct rw_semaphore is now subject to priority inheritance, and only one task at a time may read-hold. However, that task may recursively read-acquire the rw_semaphore.
struct compat_rw_semaphore
A variant of struct rw_semaphore that is -not- subject to priority inheritance. Again, this is useful for cases when you need an event mechanism, rather than a sleeplock.
Quick Quiz #3: Why can't event mechanisms use priority inheritance?
Per-CPU Variables
DEFINE_PER_CPU_LOCKED(type, name)
DECLARE_PER_CPU_LOCKED(type, name)
Define/declare a per-CPU variable with the specified type and name, but also define/declare a corresponding spinlock_t. If you have a group of per-CPU variables that you want to be protected by a spinlock, you can always group them into a struct.
get_per_cpu_locked(var, cpu)
Return the specified per-CPU variable for the specified CPU, but only after acquiring the corresponding spinlock.
put_per_cpu_locked(var, cpu)
Release the spinlock corresponding to the specified per-CPU variable for the specified CPU.
per_cpu_lock(var, cpu)
Returns the spinlock corresponding to the specified per-CPU variable for the specified CPU, but as an lvalue. This can be useful when invoking a function that takes as an argument a spinlock that it will release.
per_cpu_locked(var, cpu)
Returns the specified per-CPU variable for the specified CPU as an lvalue, but without acquiring the lock, presumably because you have already acquired the lock but need to get another reference to the variable. Or perhaps because you are making an RCU-read-side reference to the variable, and therefore do not need to acquire the lock.
Interrupt Handlers
SA_NODELAY
Used in the struct irqaction to specify that the corresponding interrupt handler should be directly invoked in hardware-interrupt context rather than being handed off to an irq thread. The function redirect_hardirq() does the wakeup, and the interrupt-processing loop may be found in do_irqd().
Note that SA_NODELAY should -not- be used for normal device interrupts: (1) this will degrade both interrupt and scheduling latency and (2) SA_NODELAY interrupt handlers are much more difficult to code and maintain than are normal interrupt handlers. Use SA_NODELAY only for low-level interrupts (such as the timer tick) or for hardware interrupts that must be processed with extreme realtime latencies.

local_irq_enable()
local_irq_disable()
local_irq_save(flags)
local_irq_restore(flags)
irqs_disabled()
irqs_disabled_flags()
local_save_flags(flags)
The local_irq*() functions do not actually disable hardware interrupts, instead, they simply disable preemption. These are suitable for use with normal interrupts, but not for SA_NODELAY interrupt handlers.
However, it is usually even better to use locks (possibly per-CPU locks) instead of these functions for PREEMPT_RT environments -- but please also consider the effects on SMP machines using non-PREEMPT kernels!

raw_local_irq_enable()
raw_local_irq_disable()
raw_local_irq_save(flags)
raw_local_irq_restore(flags)
raw_irqs_disabled()
raw_irqs_disabled_flags()
raw_local_save_flags(flags)
These functions disable hardware interrupts, and are therefore suitable for use with SA_NODELAY interrupts such as the scheduler clock interrupt (which, among other things, invokes scheduler_tick()).
These functions are quite specialized, and should only be used in low-level code such as the scheduler, synchronization primitives, and so on. Keep in mind that you cannot acquire normal spinlock_t locks while under the effects of raw_local_irq*().

Miscellaneous
wait_for_timer()
Wait for the specified timer to expire. This is required because timers run in process in the PREEMPT_RT environment, and can therefore be preempted, and can also block, for example during spinlock_t acquisition.
smp_send_reschedule_allbutself()
Sends reschedule IPI to all other CPUs. This is used in the scheduler to quickly find another CPU to run a newly awakened realtime task that is high priority, but not sufficiently high priority to run on the current CPU. This capability is necessary to do the efficient global scheduling required for realtime. Non-realtime tasks continue to be scheduled in the traditional manner per-CPU manner, sacrificing some priority exactness for greater efficiency and scalability.
INIT_FS(name)
This now takes the name of the variable as an argument so that the internal rwlock_t can be properly initialized (given the need for priority inheritance).
local_irq_disable_nort()
local_irq_enable_nort()
local_irq_save_nort(flags)
local_irq_restore_nort(flags)
spin_lock_nort(lock)
spin_unlock_nort(lock)
spin_lock_bh_nort(lock)
spin_unlock_bh_nort(lock)
BUG_ON_NONRT()
WARN_ON_NONRT()
These do nothing (or almost nothing) in PREEMPT_RT, but have the normal effect in other environments. These primitives should not be used outside of low-level code (e.g., in the scheduler, synchronization primitives, or architecture-specific code).
spin_lock_rt(lock)
spin_unlock_rt(lock)
in_atomic_rt()
BUG_ON_RT()
WARN_ON_RT()
Conversely, these have the normal effect in PREEMPT_RT, but do nothing in other environments. Again, these primitives should not be used outside of low-level code (e.g., in the scheduler, synchronization primitives, or architecture-specific code).
smp_processor_id_rt(cpu)
This returns "cpu" in the PREEMPT_RT environment, but acts the same as smp_processor_id() in other environments. This is intended for use only in the slab allocator.
PREEMPT_RT configuration options
High-Level Preemption-Option Selection

PREEMPT_NONE selects the traditional no-preemption case for server workloads.
PREEMPT_VOLUNTARY enables voluntary preemption points, but not wholesale kernel preemption. This is intended for desktop use.
PREEMPT_DESKTOP enables voluntary preemption points along with non-critical-section preemption (PREEMPT). This is intended for low-latency desktop use.
PREEMPT_RT enables full preemption, including critical sections.
Feature-Selection Configuration Options

PREEMPT enables non-critical-section kernel preemption.
PREEMPT_BKL causes big-kernel-lock critical sections to be preemptible.
PREEMPT_HARDIRQS causes hardirqs to run in process context, thus making them preemptible. However, the irqs marked as SA_NODELAY will continue to run in hardware interrupt context.
PREEMPT_RCU causes RCU read-side critical sections to be preemptible.
PREEMPT_SOFTIRQS causes softirqs to run in process context, thus making them preemptible.
Debugging Configuration Options

These are subject to change, but give a rough idea of the sorts of debug features available within PREEMPT_RT.

CRITICAL_PREEMPT_TIMING measures the maximum time that the kernel spends with preemption disabled.
CRITICAL_IRQSOFF_TIMING measures the maximum time that the kernel spends with hardware irqs disabled.
DEBUG_IRQ_FLAGS causes the kernel to validate the "flags" argument to spin_unlock_irqrestore() and similar primitives.
DEBUG_RT_LOCKING_MODE enables runtime switching of spinlocks from preemptible to non-preemptible. This is useful to kernel developers who want to evaluate the overhead of the PREEMPT_RT mechanisms.
DETECT_SOFTLOCKUP causes the kernel to dump the current stack trace of any process that spends more than 10 seconds in the kernel without rescheduling.
LATENCY_TRACE records function-call traces representing long-latency events. These traces may be read out of the kernel via /proc/latency_trace. It is possible to filter out low-latency traces via /proc/sys/kernel/preempt_thresh.
This config option is extremely useful when tracking down excessive latencies.

LPPTEST enables a device driver that performs parallel-port based latency measurements, such as used by Kristian Benoit for measurements posted on LKML in June 2005.
Use scripts/testlpp.c to actually run this test.

PRINTK_IGNORE_LOGLEVEL causes -all- printk() messages to be dumped to the console. Normally a very bad idea, but helpful when other debugging tools fail.
RT_DEADLOCK_DETECT finds deadlock cycles.
RTC_HISTOGRAM generates data for latency histograms for applications using /dev/rtc.
WAKEUP_TIMING measures the maximum time from when a high-priority thread is awakened to the time it actually starts running in microseconds. The result is accessed from /proc/sys/kernel/wakeup_timing. and the test may be restarted via:
	echo 0 > /proc/sys/kernel/preempt_max_latency
Some unintended side-effects of PREEMPT_RT
Because the PREEMPT_RT environment relies heavily on Linux being coded in an SMP-safe manner, use of PREEMPT_RT has flushed out a number of SMP bugs in the Linux kernel, including some timer deadlocks, lock omissions in ns83820_tx_timeout() and friends, an ACPI-idle scheduling latency bug, a core networking locking bug, and a number of preempt-off-needed bugs in the block IO statistics code.

Quick quiz answers
Quick Quiz #1:	How can semaphore critical sections be preempted in a non-preemptible kernel?

Strictly speaking, preemption simply does not happen in a non-preemptible kernel (e.g., non-CONFIG_PREEMPT). However, roughly the same thing can occur due to things like page faults while accessing user data, as well as via explicit calls to the scheduler.
Quick Quiz #2:	What is a simple and fast way to implement priority inheritance from writers to multiple readers?

If you come up with a way of doing this, I expect that Ingo Molnar will be very interested in learning about it. However, please check the LKML archives before getting too excited, as this problem is extremely non-trivial, there are no known solutions, and it has been discussed quite thoroughly. In particular, when thinking about writer-to-reader priority boosting, consider the case where a reader-writer lock is read-held by numerous readers, and each reader is blocked attempting to write-acquire some other reader-writer lock, each of which again is read-held by numerous readers. Of course, the time required to boost (then un-boost) all these readers counts against your scheduling latency.
Of course, one solution would be to convert the offending code sequences to use RCU. ;-) [Sorry, couldn't resist!!!]

Quick Quiz #3: Why can't event mechanisms use priority inheritance?

There is no way for Linux to figure out which task to boost. With sleeping locks, the task that acquired the semaphore would presumably be the task that will release it, so that is the task whose priority gets boosted. In contrast, with events, any task might do the down() that awakens the high-priority task.
[Thanks to Ingo Molnar for his thorough review of a previous draft of this document].


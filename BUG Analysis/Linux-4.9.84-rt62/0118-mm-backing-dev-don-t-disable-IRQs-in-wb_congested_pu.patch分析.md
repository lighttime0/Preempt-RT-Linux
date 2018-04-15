# 2018-04-12 0118-mm-backing-dev-don-t-disable-IRQs-in-wb_congested_pu.patch分析

## 1 patch原文

```
From 81f4c980b9acf2fecc6ec14797e6853bd83b1534 Mon Sep 17 00:00:00 2001
From: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
Date: Fri, 5 Feb 2016 12:17:14 +0100
Subject: [PATCH 118/351] mm: backing-dev: don't disable IRQs in
 wb_congested_put()

it triggers:
|BUG: sleeping function called from invalid context at kernel/locking/rtmutex.c:930
|in_atomic(): 0, irqs_disabled(): 1, pid: 12, name: rcuc/0
|1 lock held by rcuc/0/12:
| #0:  (rcu_callback){......}, at: [<ffffffff810ce1a6>] rcu_cpu_kthread+0x376/0xb10
|irq event stamp: 23636
|hardirqs last  enabled at (23635): [<ffffffff8173524c>] _raw_spin_unlock_irqrestore+0x6c/0x80
|hardirqs last disabled at (23636): [<ffffffff81173918>] wb_congested_put+0x18/0x90
| [<ffffffff81735434>] rt_spin_lock+0x24/0x60
| [<ffffffff810afed2>] atomic_dec_and_spin_lock+0x52/0x90
| [<ffffffff81173928>] wb_congested_put+0x28/0x90
| [<ffffffff813b833e>] __blkg_release_rcu+0x5e/0x1e0
| [<ffffffff813b8367>] ? __blkg_release_rcu+0x87/0x1e0
| [<ffffffff813b82e0>] ? blkg_conf_finish+0x90/0x90
| [<ffffffff810ce1e7>] rcu_cpu_kthread+0x3b7/0xb10

due to cgwb_lock beeing taken with spin_lock_irqsave() usually.

Signed-off-by: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
---
 mm/backing-dev.c | 4 ++--
 1 file changed, 2 insertions(+), 2 deletions(-)

diff --git a/mm/backing-dev.c b/mm/backing-dev.c
index 6ff2d7744223..b5a91dd53b5f 100644
--- a/mm/backing-dev.c
+++ b/mm/backing-dev.c
@@ -457,9 +457,9 @@ void wb_congested_put(struct bdi_writeback_congested *congested)
 {
 	unsigned long flags;
 
-	local_irq_save(flags);
+	local_irq_save_nort(flags);
 	if (!atomic_dec_and_lock(&congested->refcnt, &cgwb_lock)) {
-		local_irq_restore(flags);
+		local_irq_restore_nort(flags);
 		return;
 	}
 
-- 
2.16.1
```

从patch原文中可以确定要分析的函数——`atomic_dec_and_lock`。但是这里没有写BUG是在哪个版本的kernel发生的，所以只能从被分析对象——Linux-4.9.84-rt62来分析。（注意，kernel版本很重要的，版本之间代码不同，会使得Call Trace不同，可能分析不出Call Trace中的序列。就算Call Trace相同，BUG出现的位置也不一定相同，因为代码行数信息不可信了）。

出问题的代码位于`linux-4.9.84-rt62/mm/backing-dev.c`中的`wb_congested_put`函数中，代码如下：

![wb_congested_put](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-wb_congested_put.jpg)


## 2 函数调用路径重现

### 2.1 `atomic_dec_and_lock`

`atomic_dec_and_lock`是一个宏，定义在`linux-4.9.84-rt62/include/linux/spinlock_rt.h`中：

![atomic_dec_and_lock](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-atomic_dec_and_lock.png)

### 2.2 `atomic_dec_and_spin_lock`

`atomic_dec_and_spin_lock`函数定义在`linux-4.9.84-rt62/kernel/locking/rtmutex.c`中：

![atomic_dec_and_spin_lock](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-atomic_dec_and_spin_lock.jpg)

这个函数的语义是：

* 如果`atomic`不为1，就将它原子性的减1，成功的话返回1；
* 如果`atomic`为1，或者原子性减1操作失败，返回0。

下面分两部分分析，第一部分分析`atomic_add_unless`和`atomic_dec_and_test`函数来分析语义；第二部分分析`rt_spin_lock`来分析所处的环境。

#### 2.2.1 `atomic_dec_and_spin_lock`函数语义分析

##### 2.2.1.1 `atomic_add_unless`

`atomic_add_unless`定义在`linux-4.9.84-rt62/include/linux/atomic.h`中：

![atomic_aad_unless](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-atomic_aad_unless.jpg)

`__atomic_add_unless`是体系结构相关的代码，我们用x86中的实现来分析，代码在`linux/arch/x86/include/asm/atomic.h`中：

![__atomic_add_unless](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-__atomic_add_unless.jpg)

这样可以总结出`atomic_add_unless`函数的语义：

* 如果v的值等于u，不做任何操作，返回0；
* 如果v的值不等于u，对v原子性的加a。

原子操作会在访存时把总线锁住，这样同一总线上别的CPU就暂时不能通过总线访问内存了，保证了这条指令在多处理器环境中的原子性。

##### 2.2.1.1 `atomic_dec_and_test`

`atomic_dec_and_test`也是体系结构相关的代码，我们用x86中的实现来分析，代码在`linux/arch/x86/include/asm/atomic.h`中

![atomic_dec_and_test](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-atomic_dec_and_test.jpg)

注释写的很清楚，会将v原子性的减1，并返回一个bool值表示操作是否成功。

#### 2.2.2 `atomic_dec_and_spin_lock`函数环境分析

从Patch中记录的OOPs信息中我们看到，Call Trace的最顶上一层调用是`rt_spin_lock`，所以BUG就出在这里。

##### 2.2.2.1 `rt_spin_lock`

`rt_spin_lock`函数定义在`linux-4.9.84-rt62/kernel/locking/rtmutex.c`中：

![rt_spin_lock-linux-4.9.84-rt62](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-rt_spin_lock-linux-4.9.84-rt62.jpg)

这里有两条语句，因为Call Trace中`rt_spin_lock`是最后一层调用，所以这两条语句如果不是宏或者内嵌函数，就一定不是出BUG的地方，这样可以进一步定位BUG出现在哪里。

* （1）`rt_spin_lock_fastlock`

`rt_spin_lock_fastlock`函数同样定义在`linux-4.9.84-rt62/kernel/locking/rtmutex.c`中：

![rt_spin_lock_fastlock-linux-4.9.84-rt62](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-rt_spin_lock_fastlock-linux-4.9.84-rt62.jpg)

这是一个内嵌函数，BUG可能出现在这里。

* （2）`spin_acquire`

`spin_acquire`宏定义在`linux-4.9.84-rt62/include/linux/lockdep.h`中：

![spin_acquire-linux-4.9.84-rt62](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-spin_acquire-linux-4.9.84-rt62.jpg)

在同一个文件中：

![lock_acquire_exclusive-linux-4.9.84-rt62](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-lock_acquire_exclusive-linux-4.9.84-rt62.jpg)

`lock_acquire`函数定义在`linux-4.9.84-rt62/kernel/locking/lockdep.h`中：

![lock_acquire-linux-4.9.84-rt62](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-lock_acquire-linux-4.9.84-rt62.jpg)

这个宏展开到最后，发现是一个函数调用，这个函数调用没有出现在Call Trace中，所以不是出BUG的地方。

* 总结（1）和（2）

可以得到结论，出BUG的代码在`rt_spin_lock_fastlock`中。可以看到，`rt_spin_lock_fastlock`的第一条语句是`might_sleep_no_state_check();`：

![might_sleep_*-linux-4.9.84-rt62](media/15235243939595/might_sleep_*-linux-4.9.84-rt62.jpg)

这个宏用来放在kernel中，提醒开发人员这里可能会睡眠。

## 3 函数执行上下文分析

回到一开始Patch中的OOPs信息中去，`atomic_dec_and_lock`是在`local_irq_save()`的环境中执行的，从`linux-4.8.84-rt62中0089-usb-Use-_nort-in-giveback-function.patch的分析`能看出，这是一个关中断的环境。

## 4 结论

综合以上分析，这个BUG的原因是在本地硬中断关闭的环境下睡眠了。





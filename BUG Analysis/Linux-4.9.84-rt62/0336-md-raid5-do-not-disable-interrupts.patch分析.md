# 0336-md-raid5-do-not-disable-interrupts.patch分析

## 1 Patch原文

```patch
From 2c847daa4408b2ffae4972e3ade9b021c1faf5f0 Mon Sep 17 00:00:00 2001
From: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
Date: Fri, 17 Nov 2017 16:21:00 +0100
Subject: [PATCH 336/351] md/raid5: do not disable interrupts

|BUG: sleeping function called from invalid context at kernel/locking/rtmutex.c:974
|in_atomic(): 0, irqs_disabled(): 1, pid: 2992, name: lvm
|CPU: 2 PID: 2992 Comm: lvm Not tainted 4.13.10-rt3+ #54
|Call Trace:
| dump_stack+0x4f/0x65
| ___might_sleep+0xfc/0x150
| atomic_dec_and_spin_lock+0x3c/0x80
| raid5_release_stripe+0x73/0x110
| grow_one_stripe+0xce/0xf0
| setup_conf+0x841/0xaa0
| raid5_run+0x7e7/0xa40
| md_run+0x515/0xaf0
| raid_ctr+0x147d/0x25e0
| dm_table_add_target+0x155/0x320
| table_load+0x103/0x320
| ctl_ioctl+0x1d9/0x510
| dm_ctl_ioctl+0x9/0x10
| do_vfs_ioctl+0x8e/0x670
| SyS_ioctl+0x3c/0x70
| entry_SYSCALL_64_fastpath+0x17/0x98

The interrupts were disabled because ->device_lock is taken with
interrupts disabled.

Cc: stable-rt@vger.kernel.org
Signed-off-by: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
Signed-off-by: Steven Rostedt (VMware) <rostedt@goodmis.org>
---
 drivers/md/raid5.c | 4 ++--
 1 file changed, 2 insertions(+), 2 deletions(-)

diff --git a/drivers/md/raid5.c b/drivers/md/raid5.c
index 710bb5d009a8..8d2c9d70042e 100644
--- a/drivers/md/raid5.c
+++ b/drivers/md/raid5.c
@@ -429,7 +429,7 @@ void raid5_release_stripe(struct stripe_head *sh)
 		md_wakeup_thread(conf->mddev->thread);
 	return;
 slow_path:
-	local_irq_save(flags);
+	local_irq_save_nort(flags);
 	/* we are ok here if STRIPE_ON_RELEASE_LIST is set or not */
 	if (atomic_dec_and_lock(&sh->count, &conf->device_lock)) {
 		INIT_LIST_HEAD(&list);
@@ -438,7 +438,7 @@ void raid5_release_stripe(struct stripe_head *sh)
 		spin_unlock(&conf->device_lock);
 		release_inactive_stripe_list(conf, &list, hash);
 	}
-	local_irq_restore(flags);
+	local_irq_restore_nort(flags);
 }
 
 static inline void remove_hash(struct stripe_head *sh)
-- 
2.16.1
```

首先，让我们在`linux-4.9.84`中定位一下要修改的代码：

![raid5_release_stripe](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-raid5_release_stripe.png)

从OOPs的信息中可以看到，当时kernel版本为linux-4.13.10-rt3，所以我们切换到这个版本的kernel和RTpatch去分析。

## 2 分析

从Call Trace中可以看到，出问题的函数在上图中的`atomic_dec_and_lock`中，根据`0118-mm-backing-dev-don-t-disable-IRQs-in-wb_congested_pu.patch分析`中对`atomic_dec_and_lock`的分析可以知道，这里的BUG也是因为在cli的环境下睡眠引起的。

## 3 结论

综合以上分析，这个BUG的原因是在本地硬中断关闭的环境下睡眠了。


# 0250-snd-pcm-fix-snd_pcm_stream_lock-irqs_disabled-splats.patch分析

## 1 Patch原文

```patch
From 68189d78de6f65c38ff6681790f68f96193bd77a Mon Sep 17 00:00:00 2001
From: Mike Galbraith <umgwanakikbuti@gmail.com>
Date: Wed, 18 Feb 2015 15:09:23 +0100
Subject: [PATCH 250/351] snd/pcm: fix snd_pcm_stream_lock*() irqs_disabled()
 splats

Locking functions previously using read_lock_irq()/read_lock_irqsave() were
changed to local_irq_disable/save(), leading to gripes.  Use nort variants.

|BUG: sleeping function called from invalid context at kernel/locking/rtmutex.c:915
|in_atomic(): 0, irqs_disabled(): 1, pid: 5947, name: alsa-sink-ALC88
|CPU: 5 PID: 5947 Comm: alsa-sink-ALC88 Not tainted 3.18.7-rt1 #9
|Hardware name: MEDION MS-7848/MS-7848, BIOS M7848W08.404 11/06/2014
| ffff880409316240 ffff88040866fa38 ffffffff815bdeb5 0000000000000002
| 0000000000000000 ffff88040866fa58 ffffffff81073c86 ffffffffa03b2640
| ffff88040239ec00 ffff88040866fa78 ffffffff815c3d34 ffffffffa03b2640
|Call Trace:
| [<ffffffff815bdeb5>] dump_stack+0x4f/0x9e
| [<ffffffff81073c86>] __might_sleep+0xe6/0x150
| [<ffffffff815c3d34>] __rt_spin_lock+0x24/0x50
| [<ffffffff815c4044>] rt_read_lock+0x34/0x40
| [<ffffffffa03a2979>] snd_pcm_stream_lock+0x29/0x70 [snd_pcm]
| [<ffffffffa03a355d>] snd_pcm_playback_poll+0x5d/0x120 [snd_pcm]
| [<ffffffff811937a2>] do_sys_poll+0x322/0x5b0
| [<ffffffff81193d48>] SyS_ppoll+0x1a8/0x1c0
| [<ffffffff815c4556>] system_call_fastpath+0x16/0x1b

Signed-off-by: Mike Galbraith <umgwanakikbuti@gmail.com>
Signed-off-by: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
---
 sound/core/pcm_native.c | 8 ++++----
 1 file changed, 4 insertions(+), 4 deletions(-)

diff --git a/sound/core/pcm_native.c b/sound/core/pcm_native.c
index 9d33c1e85c79..3d307bda86f9 100644
--- a/sound/core/pcm_native.c
+++ b/sound/core/pcm_native.c
@@ -135,7 +135,7 @@ EXPORT_SYMBOL_GPL(snd_pcm_stream_unlock);
 void snd_pcm_stream_lock_irq(struct snd_pcm_substream *substream)
 {
 	if (!substream->pcm->nonatomic)
-		local_irq_disable();
+		local_irq_disable_nort();
 	snd_pcm_stream_lock(substream);
 }
 EXPORT_SYMBOL_GPL(snd_pcm_stream_lock_irq);
@@ -150,7 +150,7 @@ void snd_pcm_stream_unlock_irq(struct snd_pcm_substream *substream)
 {
 	snd_pcm_stream_unlock(substream);
 	if (!substream->pcm->nonatomic)
-		local_irq_enable();
+		local_irq_enable_nort();
 }
 EXPORT_SYMBOL_GPL(snd_pcm_stream_unlock_irq);
 
@@ -158,7 +158,7 @@ unsigned long _snd_pcm_stream_lock_irqsave(struct snd_pcm_substream *substream)
 {
 	unsigned long flags = 0;
 	if (!substream->pcm->nonatomic)
-		local_irq_save(flags);
+		local_irq_save_nort(flags);
 	snd_pcm_stream_lock(substream);
 	return flags;
 }
@@ -176,7 +176,7 @@ void snd_pcm_stream_unlock_irqrestore(struct snd_pcm_substream *substream,
 {
 	snd_pcm_stream_unlock(substream);
 	if (!substream->pcm->nonatomic)
-		local_irq_restore(flags);
+		local_irq_restore_nort(flags);
 }
 EXPORT_SYMBOL_GPL(snd_pcm_stream_unlock_irqrestore);
 
-- 
2.16.1
```

从OOPs的信息中可以看到，当时kernel版本为linux-3.18.7-rt1，所以我们切换到这个版本的kernel和RTpatch去分析。

## 2 BUG分析

从Call Trace可以看到，出问题的函数调用是这两行：

![snd_pcm_stream_lock_irq-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-snd_pcm_stream_lock_irq-linux-3.18.7-rt1.png)

`snd_pcm_stream_lock`函数的定义如下：

![snd_pcm_stream_lock-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-snd_pcm_stream_lock-linux-3.18.7-rt1.png)

Call Trace中的下一层调用是`rt_read_lock`，那么应该是从这里的`read_lock`宏展开的：

![read_lock-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-read_lock-linux-3.18.7-rt1.png)

这下就到了`rt_read_lock`函数：

![rt_read_lock-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-rt_read_lock-linux-3.18.7-rt1.png)

接下来就到了Call Trace中出现的`__rt_spin_lock`：

![__rt_spin_lock-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-__rt_spin_lock-linux-3.18.7-rt1.png)

`rt_spin_lock_fastlock`：

![rt_spin_lock_fastlock-linux-3.18.7-rt1](http://oy60g0sqx.bkt.clouddn.com/2018-04-15-rt_spin_lock_fastlock-linux-3.18.7-rt1.png)

根据Call Trace中的信息，下一层调用是`__might_sleep`，应该是从这里的`might_sleep`宏展开的：

![might_sleep-linux-3.18.7-rt1](media/15237621608334/might_sleep-linux-3.18.7-rt1.png)

## 3 Context分析

从`linux-4.8.84-rt62中0089-usb-Use-_nort-in-giveback-function.patch的分析`能看出，`local_irq_save`会造成一个关中断的环境。这里不应该睡眠。

## 4 结论

综合以上分析，这个BUG的原因是在本地硬中断关闭的环境下睡眠了。


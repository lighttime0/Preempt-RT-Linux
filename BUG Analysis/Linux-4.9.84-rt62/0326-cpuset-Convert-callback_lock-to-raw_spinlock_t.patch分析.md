# 0336-md-raid5-do-not-disable-interrupts.patch分析

## 1 Patch原文

```patch
From 1c8716714af1ce3ceebfb1e6f132135f2ffddd7f Mon Sep 17 00:00:00 2001
From: Mike Galbraith <efault@gmx.de>
Date: Sun, 8 Jan 2017 09:32:25 +0100
Subject: [PATCH 326/351] cpuset: Convert callback_lock to raw_spinlock_t

The two commits below add up to a cpuset might_sleep() splat for RT:

8447a0fee974 cpuset: convert callback_mutex to a spinlock
344736f29b35 cpuset: simplify cpuset_node_allowed API

BUG: sleeping function called from invalid context at kernel/locking/rtmutex.c:995
in_atomic(): 0, irqs_disabled(): 1, pid: 11718, name: cset
CPU: 135 PID: 11718 Comm: cset Tainted: G            E   4.10.0-rt1-rt #4
Hardware name: Intel Corporation BRICKLAND/BRICKLAND, BIOS BRHSXSD1.86B.0056.R01.1409242327 09/24/2014
Call Trace:
 ? dump_stack+0x5c/0x81
 ? ___might_sleep+0xf4/0x170
 ? rt_spin_lock+0x1c/0x50
 ? __cpuset_node_allowed+0x66/0xc0
 ? ___slab_alloc+0x390/0x570 <disables IRQs>
 ? anon_vma_fork+0x8f/0x140
 ? copy_page_range+0x6cf/0xb00
 ? anon_vma_fork+0x8f/0x140
 ? __slab_alloc.isra.74+0x5a/0x81
 ? anon_vma_fork+0x8f/0x140
 ? kmem_cache_alloc+0x1b5/0x1f0
 ? anon_vma_fork+0x8f/0x140
 ? copy_process.part.35+0x1670/0x1ee0
 ? _do_fork+0xdd/0x3f0
 ? _do_fork+0xdd/0x3f0
 ? do_syscall_64+0x61/0x170
 ? entry_SYSCALL64_slow_path+0x25/0x25

The later ensured that a NUMA box WILL take callback_lock in atomic
context by removing the allocator and reclaim path __GFP_HARDWALL
usage which prevented such contexts from taking callback_mutex.

One option would be to reinstate __GFP_HARDWALL protections for
RT, however, as the 8447a0fee974 changelog states:

The callback_mutex is only used to synchronize reads/updates of cpusets'
flags and cpu/node masks. These operations should always proceed fast so
there's no reason why we can't use a spinlock instead of the mutex.

Cc: stable-rt@vger.kernel.org
Signed-off-by: Mike Galbraith <efault@gmx.de>
Signed-off-by: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
---
 kernel/cpuset.c | 66 ++++++++++++++++++++++++++++-----------------------------
 1 file changed, 33 insertions(+), 33 deletions(-)

diff --git a/kernel/cpuset.c b/kernel/cpuset.c
index 511b1dd8ff09..1dd63833ecdc 100644
--- a/kernel/cpuset.c
+++ b/kernel/cpuset.c
@@ -285,7 +285,7 @@ static struct cpuset top_cpuset = {
  */
 
 static DEFINE_MUTEX(cpuset_mutex);
-static DEFINE_SPINLOCK(callback_lock);
+static DEFINE_RAW_SPINLOCK(callback_lock);
 
 static struct workqueue_struct *cpuset_migrate_mm_wq;
 
@@ -908,9 +908,9 @@ static void update_cpumasks_hier(struct cpuset *cs, struct cpumask *new_cpus)
 			continue;
 		rcu_read_unlock();
 
-		spin_lock_irq(&callback_lock);
+		raw_spin_lock_irq(&callback_lock);
 		cpumask_copy(cp->effective_cpus, new_cpus);
-		spin_unlock_irq(&callback_lock);
+		raw_spin_unlock_irq(&callback_lock);
 
 		WARN_ON(!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
 			!cpumask_equal(cp->cpus_allowed, cp->effective_cpus));
@@ -975,9 +975,9 @@ static int update_cpumask(struct cpuset *cs, struct cpuset *trialcs,
 	if (retval < 0)
 		return retval;
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cpumask_copy(cs->cpus_allowed, trialcs->cpus_allowed);
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	/* use trialcs->cpus_allowed as a temp variable */
 	update_cpumasks_hier(cs, trialcs->cpus_allowed);
@@ -1177,9 +1177,9 @@ static void update_nodemasks_hier(struct cpuset *cs, nodemask_t *new_mems)
 			continue;
 		rcu_read_unlock();
 
-		spin_lock_irq(&callback_lock);
+		raw_spin_lock_irq(&callback_lock);
 		cp->effective_mems = *new_mems;
-		spin_unlock_irq(&callback_lock);
+		raw_spin_unlock_irq(&callback_lock);
 
 		WARN_ON(!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
 			!nodes_equal(cp->mems_allowed, cp->effective_mems));
@@ -1247,9 +1247,9 @@ static int update_nodemask(struct cpuset *cs, struct cpuset *trialcs,
 	if (retval < 0)
 		goto done;
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cs->mems_allowed = trialcs->mems_allowed;
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	/* use trialcs->mems_allowed as a temp variable */
 	update_nodemasks_hier(cs, &trialcs->mems_allowed);
@@ -1340,9 +1340,9 @@ static int update_flag(cpuset_flagbits_t bit, struct cpuset *cs,
 	spread_flag_changed = ((is_spread_slab(cs) != is_spread_slab(trialcs))
 			|| (is_spread_page(cs) != is_spread_page(trialcs)));
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cs->flags = trialcs->flags;
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	if (!cpumask_empty(trialcs->cpus_allowed) && balance_flag_changed)
 		rebuild_sched_domains_locked();
@@ -1757,7 +1757,7 @@ static int cpuset_common_seq_show(struct seq_file *sf, void *v)
 	cpuset_filetype_t type = seq_cft(sf)->private;
 	int ret = 0;
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 
 	switch (type) {
 	case FILE_CPULIST:
@@ -1776,7 +1776,7 @@ static int cpuset_common_seq_show(struct seq_file *sf, void *v)
 		ret = -EINVAL;
 	}
 
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 	return ret;
 }
 
@@ -1991,12 +1991,12 @@ static int cpuset_css_online(struct cgroup_subsys_state *css)
 
 	cpuset_inc();
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys)) {
 		cpumask_copy(cs->effective_cpus, parent->effective_cpus);
 		cs->effective_mems = parent->effective_mems;
 	}
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	if (!test_bit(CGRP_CPUSET_CLONE_CHILDREN, &css->cgroup->flags))
 		goto out_unlock;
@@ -2023,12 +2023,12 @@ static int cpuset_css_online(struct cgroup_subsys_state *css)
 	}
 	rcu_read_unlock();
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cs->mems_allowed = parent->mems_allowed;
 	cs->effective_mems = parent->mems_allowed;
 	cpumask_copy(cs->cpus_allowed, parent->cpus_allowed);
 	cpumask_copy(cs->effective_cpus, parent->cpus_allowed);
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 out_unlock:
 	mutex_unlock(&cpuset_mutex);
 	return 0;
@@ -2067,7 +2067,7 @@ static void cpuset_css_free(struct cgroup_subsys_state *css)
 static void cpuset_bind(struct cgroup_subsys_state *root_css)
 {
 	mutex_lock(&cpuset_mutex);
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 
 	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys)) {
 		cpumask_copy(top_cpuset.cpus_allowed, cpu_possible_mask);
@@ -2078,7 +2078,7 @@ static void cpuset_bind(struct cgroup_subsys_state *root_css)
 		top_cpuset.mems_allowed = top_cpuset.effective_mems;
 	}
 
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 	mutex_unlock(&cpuset_mutex);
 }
 
@@ -2179,12 +2179,12 @@ hotplug_update_tasks_legacy(struct cpuset *cs,
 {
 	bool is_empty;
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cpumask_copy(cs->cpus_allowed, new_cpus);
 	cpumask_copy(cs->effective_cpus, new_cpus);
 	cs->mems_allowed = *new_mems;
 	cs->effective_mems = *new_mems;
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	/*
 	 * Don't call update_tasks_cpumask() if the cpuset becomes empty,
@@ -2221,10 +2221,10 @@ hotplug_update_tasks(struct cpuset *cs,
 	if (nodes_empty(*new_mems))
 		*new_mems = parent_cs(cs)->effective_mems;
 
-	spin_lock_irq(&callback_lock);
+	raw_spin_lock_irq(&callback_lock);
 	cpumask_copy(cs->effective_cpus, new_cpus);
 	cs->effective_mems = *new_mems;
-	spin_unlock_irq(&callback_lock);
+	raw_spin_unlock_irq(&callback_lock);
 
 	if (cpus_updated)
 		update_tasks_cpumask(cs);
@@ -2317,21 +2317,21 @@ static void cpuset_hotplug_workfn(struct work_struct *work)
 
 	/* synchronize cpus_allowed to cpu_active_mask */
 	if (cpus_updated) {
-		spin_lock_irq(&callback_lock);
+		raw_spin_lock_irq(&callback_lock);
 		if (!on_dfl)
 			cpumask_copy(top_cpuset.cpus_allowed, &new_cpus);
 		cpumask_copy(top_cpuset.effective_cpus, &new_cpus);
-		spin_unlock_irq(&callback_lock);
+		raw_spin_unlock_irq(&callback_lock);
 		/* we don't mess with cpumasks of tasks in top_cpuset */
 	}
 
 	/* synchronize mems_allowed to N_MEMORY */
 	if (mems_updated) {
-		spin_lock_irq(&callback_lock);
+		raw_spin_lock_irq(&callback_lock);
 		if (!on_dfl)
 			top_cpuset.mems_allowed = new_mems;
 		top_cpuset.effective_mems = new_mems;
-		spin_unlock_irq(&callback_lock);
+		raw_spin_unlock_irq(&callback_lock);
 		update_tasks_nodemask(&top_cpuset);
 	}
 
@@ -2436,11 +2436,11 @@ void cpuset_cpus_allowed(struct task_struct *tsk, struct cpumask *pmask)
 {
 	unsigned long flags;
 
-	spin_lock_irqsave(&callback_lock, flags);
+	raw_spin_lock_irqsave(&callback_lock, flags);
 	rcu_read_lock();
 	guarantee_online_cpus(task_cs(tsk), pmask);
 	rcu_read_unlock();
-	spin_unlock_irqrestore(&callback_lock, flags);
+	raw_spin_unlock_irqrestore(&callback_lock, flags);
 }
 
 void cpuset_cpus_allowed_fallback(struct task_struct *tsk)
@@ -2488,11 +2488,11 @@ nodemask_t cpuset_mems_allowed(struct task_struct *tsk)
 	nodemask_t mask;
 	unsigned long flags;
 
-	spin_lock_irqsave(&callback_lock, flags);
+	raw_spin_lock_irqsave(&callback_lock, flags);
 	rcu_read_lock();
 	guarantee_online_mems(task_cs(tsk), &mask);
 	rcu_read_unlock();
-	spin_unlock_irqrestore(&callback_lock, flags);
+	raw_spin_unlock_irqrestore(&callback_lock, flags);
 
 	return mask;
 }
@@ -2584,14 +2584,14 @@ bool __cpuset_node_allowed(int node, gfp_t gfp_mask)
 		return true;
 
 	/* Not hardwall and node outside mems_allowed: scan up cpusets */
-	spin_lock_irqsave(&callback_lock, flags);
+	raw_spin_lock_irqsave(&callback_lock, flags);
 
 	rcu_read_lock();
 	cs = nearest_hardwall_ancestor(task_cs(current));
 	allowed = node_isset(node, cs->mems_allowed);
 	rcu_read_unlock();
 
-	spin_unlock_irqrestore(&callback_lock, flags);
+	raw_spin_unlock_irqrestore(&callback_lock, flags);
 	return allowed;
 }
 
-- 
2.16.1
```

## 2 分析

结合Call Trace信息，从`linux-4.8.84-rt62中0089-usb-Use-_nort-in-giveback-function.patch的分析`能看出，`local_irq_save`会造成一个关中断的环境。这里不应该睡眠。

## 3 结论

综合以上分析，这个BUG的原因是在本地硬中断关闭的环境下睡眠了。


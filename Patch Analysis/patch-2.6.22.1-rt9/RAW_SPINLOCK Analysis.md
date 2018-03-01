# RAW_SPINLOCK

在该patch中，共有86处使用了RAW_SPINLOCK。从之前的文档中可以了解到，RAW_SPINLOCK使用了原始SPINLOCK的语义，而新的SPINLOCK则映射到rt_mutex。这样做的原因是kernel中大部分的SPINLOCK是可以抢占的，这些代码不需要修改（因为spinlock_t现在被影射到rt_mutex了），只需要把很少一部分的真的不能抢占的代码改成raw_spinlock_t就可以了。

下面对着86处RAW_SPINLOCK进行分析：

### 1 dma_spin_lock

位置：linux/arch/arm/kernel/dma.c
简介：


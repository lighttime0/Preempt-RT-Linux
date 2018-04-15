# 2018-04-09 对linux-4.8.84-rt62中0089-usb-Use-_nort-in-giveback-function.patch的深度分析

## 1 背景知识

### 1.1 kernel documentation中对usb的介绍
    
usb：

> https://www.kernel.org/doc/html/latest/driver-api/usb/usb.html#introduction-to-usb-on-linux

urb：

> https://www.kernel.org/doc/html/latest/driver-api/usb/URB.html

与本patch相关的文档摘要：

* Each URB has a completion handler, which is called after the action has been successfully completed or canceled. The URB also contains a context-pointer for passing information to the completion handler.

    每个URB都有一个completion handler，会在一次操作成功完成或者取消的时候被调用。URB还会包含一个内容指针（context-pointer），用来给completion handler传递信息。
    
* urb completion handler

    ![urb-completion-handle](http://oy60g0sqx.bkt.clouddn.com/2018-04-10-urb-completion-handler.png)


### 1.2 软中断、tasklet、Workqueue

https://blog.csdn.net/godleading/article/details/52971179

### 1.1 hcd

hcd是主机控制器的驱动程序。它位于USB主机控制器与USB系统软件之间。主机控制器可以有一系列不同的实现，而系统软件独立于任何一个具体实现。一个驱动程序可以支持不同的控制器，而不必特别了解这个具体的控制器。一个USB控制器的实现者必须提供一个支持它自己的控制器的主机控制器驱动器(HCD)实现。

## 2 BUG分析

### 2.1 OOPs原始信息

![0089-usb-OOPs](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-0089-usb-OOPs.png)

从图中我们可以看出，BUG的原因是在非法上下文中睡眠了。反映在Call Trace中，就是第二行的`__might_sleep`调用。

这个bug是这样fix的：

![bug_fix](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-bug_fix.png)

可以看到，这个bug是通过修改`__usb_hcd_giveback_urb`函数fix的，所以，我们分析的重点就是OOPs信息截图中红框里的4层调用。分析顺序和调用顺序一致，从下往上分析。

### 2.2 urb->complete(urb);

#### 2.2.1 `__usb_hcd_giveback_urb`

这个函数位于`linux-4.9.84/drivers/usb/core/hcd.c`，函数代码如下：

![__usb_hcd_giveback_urb](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-__usb_hcd_giveback_urb.png)

出问题的代码在红框里。下面我们对`urb->complete(urb)`这条语句进行分析。

URB（USB Request Block）是USB子系统中用于传输数据的主要方式，详细的介绍见[这里](https://www.kernel.org/doc/html/latest/driver-api/usb/URB.html)。和这里的分析相关的是`struct urb`中的`usb_complete_t complete`成员，`complete`是一个函数指针，也就是回调函数，在URB传输完成或者传输中断的时候被调用，参数是`struct urb`本身。这个函数由驱动程序去实现，并在创建`struct urb`的时候给`complete`赋值。

我们这里BUG的Call Trace显示，此处complete赋值为`hid_ctrl`，所以，下一步就是分析`hid_ctrl`函数了。另外，这个Call Trace发生的环境是`linux-3.12.0-rt0-rc1`，但是找不到这个版本的了，所以我找了最接近的`linux-3.12.0-rt1`，那么**接下来，对Call Trace的分析切换到`linux-3.12.0-rt1`中**。

#### 2.2.2 `hid_ctrl`

这个函数位于`linux-3.12.0-rt1/drivers/hid/usbhid/hid-core.c`，函数截图如下：

![hid_ctr](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-hid_ctrl.png)


HID是Human Interface Devices的缩写。翻译成中文即为人机交互设备。常见的HID设备有鼠标键盘、游戏操纵杆等等。这个函数就是HID驱动的complete函数。

函数出问题的代码在红框中圈出。

#### 2.2.3 `rt_spin_lock`

我们在`linux-3.12.0-rt1/include/linux/spinlock_rt.h`中找到了`spin_lock`宏的定义：

![spin_lock](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-spin_lock.png)

这个宏会执行两条函数调用，Call Trace中的记录是在第二条语句`rt_spin_lock`中出问题的，所以稍后会对它进行分析。现在，让我们首先来看看`migrate_disable`会对context造成什么影响。

##### 2.2.3.1 `migrate_disable`

`migrate_disable`函数定义在`linux-3.12.0-rt1/kernel/sched/core.c`中，这个函数只用在CONFIG_PREEMPT_RT_FULL中，用来禁止任务切换到另一个CPU：

![task_struct-migrate_disable](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-task_struct-migrate_disable.png)

![migrate_disable](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-migrate_disable.png)

根据代码逻辑：

* 如果在原子上下文中，不需要做什么。

* 如果不在原子上下文中，

    * 如果当前进程的`migrate_disable`不为0，那么自增1，返回。

    * 如果当前进程的`migrate_disable`为0，那么先调用`pin_current_cpu`函数，防止CPU unplugged，然后将`migrate_disable`赋值为1，返回。

![pin_current_cpu](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-pin_current_cpu.png)

##### 2.2.3.2 `rt_spin_lock`

`rt_spin_lock`的定义在`linux-3.12.0-rt1/kernel/locking/rtmutex.c`：

![rt_spin_lock](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-rt_spin_lock.png)

同一个文件中，能找到`rt_spin_lock_fastlock`的定义：

![rt_spin_lock_fastlock](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-rt_spin_lock_fastlock.png)

`rt_spin_lock_fastlock`是一个inline函数，会把代码直接展开到`rt_spin_lock`中，所以在Call Trace中没有出现。

**值得注意的是，`might_sleep();`这条语句的位置就是OOPs信息中BUG出现的位置——kernel/rtmutex.c:673**。也就是说，BUG的出现就是因为在这里睡眠了。

#### 2.2.4 `__might_sleep`

在`rt_spin_lock_fastlock`中第一条语句就是`might_sleep();`，这个宏定义在`linux-3.12.0-rt1/include/linux/kernel.h`中：

![might_sleep](http://oy60g0sqx.bkt.clouddn.com/2018-04-14-might_sleep.png)

红框中就是Call Trace中出现的`__might_sleep`函数。

`__might_sleep`定义和`might_sleep`在同一张图（也就是上图）中。从上图中的注释和代码我们可以看出来，`might_sleep`函数不做任何事情，只是用来在kernel代码中用来提示开发人员，这个函数可能会睡眠。

### 2.3 local_irq_save(flags);

2.2中对`urb->complete(urb);`分析了代码的运行逻辑，接下来要对`local_irq_save(flags);`进行分析，来分析BUG发生时所处的context。

#### 2.3.1 `local_irq_save(flags)`

这个宏定义在`/include/linux/irqflags.h`中，定义如下图：

![local_irq_save](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-local_irq_save.png)

#### 2.3.2 `raw_local_irq_save(flags)`

这个宏也定义在`/include/linux/irqflags.h`中，定义如下图：

![raw_local_irq_save](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-raw_local_irq_save.png)

#### 2.3.3 `arch_local_irq_save()`

这个宏定义是和体系结构相关的。BUG的OOPs中表明这个BUG是在Bochs模拟器中发现的，Bochs是用来模拟x86硬件平台的，所以在`/arch/x86`下找这个宏的定义。

这个宏的定义在`/arch/x86/include/asm/irqflags.h`，代码如下：

![arch_local_irq_save-x86](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-arch_local_irq_save-x86.png)

#### 2.3.4

##### 2.3.4.1 `arch_local_save_flags()`

这个宏的定义在`linux-4.9.84-rt62/arch/x86/include/asm/irqflags.h`，代码如下：

![arch_local_save_flags-x86](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-arch_local_save_flags-x86.png)

##### 2.3.4.2 `arch_local_irq_disable`

这个宏的定义在`linux-4.9.84-rt62/arch/x86/include/asm/irqflags.h`，代码如下：

![arch_local_irq_disable-x86](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-arch_local_irq_disable-x86.png)

#### 2.3.5

##### 2.3.5.1 `native_save_fl()`

这个宏的定义在`/arch/x86/include/asm/irqflags.h`，代码如下：

![native_save_fl-x86](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-native_save_fl-x86.png)


##### 2.3.5.2 `native_irq_disable()`

这个宏的定义在`/arch/x86/include/asm/irqflags.h`，代码如下：

![native_irq_disable-x86](http://oy60g0sqx.bkt.clouddn.com/2018-04-12-native_irq_disable-x86.png)

OK，这下可以总结出来，在BUG发生的时候，处于`cli`关中断的上下文中。

## 3 结论

综合以上分析，这个BUG的原因是在本地硬中断关闭的环境下睡眠了。





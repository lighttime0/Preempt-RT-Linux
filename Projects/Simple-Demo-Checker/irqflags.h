
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags = 0;
	return flags;
}

#define raw_local_irq_save(flags)	\
	do {							\
		arch_local_irq_save();		\
	} while(0)

#define local_irq_save(flags)		\
	do {							\
		raw_local_irq_save(flags);	\
	} while(0)
#include <stdlib.h>
#include "irqflags.h"

void __might_sleep()
{

}

void complete()
{
	__might_sleep();
}

static void arch_local_irq_restore(){

}

#define local_irq_restore arch_local_irq_restore()

void foo()
{
	unsigned long flags = 0;
	local_irq_save(flags);
	complete();
	arch_local_irq_restore();
}

int main()
{
	foo();
	return(0);
}
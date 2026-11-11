#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

void interrupts_initialize(void);
void interrupts_enable_input_irqs(void);
void interrupts_enable(void);
u32 interrupts_timer_ticks(void);
void timer_irq_handler(void);

#endif
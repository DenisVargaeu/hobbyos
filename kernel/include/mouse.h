#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

void mouse_initialize(void);
void mouse_set_bounds(u32 width, u32 height);
void mouse_get_position(i32 *x, i32 *y);
u8 mouse_get_buttons(void);
void mouse_handle_irq(void);

#endif
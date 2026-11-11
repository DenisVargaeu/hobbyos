#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

int framebuffer_initialize(u32 multiboot_info_address);
u32 framebuffer_width(void);
u32 framebuffer_height(void);
void framebuffer_draw_desktop(void);
void framebuffer_draw_pointer_status(i32 x, i32 y);
void framebuffer_note_input(char character);
void framebuffer_xor_cursor(i32 x, i32 y);
void framebuffer_handle_click(i32 x, i32 y);

#endif
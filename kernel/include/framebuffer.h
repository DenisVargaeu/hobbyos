#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

#define POWER_ACTION_NONE 0
#define POWER_ACTION_RESTART 1
#define POWER_ACTION_SHUTDOWN 2

int framebuffer_initialize(u32 multiboot_info_address);
u32 framebuffer_width(void);
u32 framebuffer_height(void);
void framebuffer_draw_boot_screen(void);
void framebuffer_draw_boot_progress(u32 step);
void framebuffer_draw_desktop(void);
void framebuffer_draw_pointer_status(i32 x, i32 y);
void framebuffer_note_input(char character);
void framebuffer_calculator_input(char character);
u8 framebuffer_take_power_action(void);
void framebuffer_xor_cursor(i32 x, i32 y);
void framebuffer_handle_click(i32 x, i32 y);

#endif
#include "include/framebuffer.h"
#include "include/interrupts.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/types.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289u

void kernel_main(u32 magic, u32 multiboot_info_address)
{
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC ||
        !framebuffer_initialize(multiboot_info_address)) {
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    framebuffer_draw_desktop();
    mouse_set_bounds(framebuffer_width(), framebuffer_height());
    mouse_initialize();
    interrupts_initialize();
    interrupts_enable_input_irqs();

    i32 cursor_x;
    i32 cursor_y;
    mouse_get_position(&cursor_x, &cursor_y);
    u8 previous_buttons = mouse_get_buttons();
    framebuffer_draw_pointer_status(cursor_x, cursor_y);
    framebuffer_xor_cursor(cursor_x, cursor_y);
    interrupts_enable();

    for (;;) {
        __asm__ volatile ("hlt");
        i32 next_x;
        i32 next_y;
        char typed_character;
        mouse_get_position(&next_x, &next_y);
        u8 buttons = mouse_get_buttons();
        int has_typed_character = keyboard_pop_char(&typed_character);
        if (next_x != cursor_x || next_y != cursor_y ||
            buttons != previous_buttons || has_typed_character) {
            framebuffer_xor_cursor(cursor_x, cursor_y);
            cursor_x = next_x;
            cursor_y = next_y;
            if ((buttons & 1u) != 0 && (previous_buttons & 1u) == 0) {
                framebuffer_handle_click(cursor_x, cursor_y);
            }
            if (has_typed_character) {
                framebuffer_note_input(typed_character);
            }
            framebuffer_draw_pointer_status(cursor_x, cursor_y);
            framebuffer_xor_cursor(cursor_x, cursor_y);
            previous_buttons = buttons;
        }
    }
}
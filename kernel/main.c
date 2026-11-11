#include "include/framebuffer.h"
#include "include/interrupts.h"
#include "include/io.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/fat16.h"
#include "include/types.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289u
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43
#define PIT_SPEAKER_PORT 0x61

static void boot_delay_tick(void)
{
    outb(PIT_COMMAND_PORT, 0xB0);
    outb(PIT_CHANNEL2_PORT, 0xEB);
    outb(PIT_CHANNEL2_PORT, 0xE8);

    for (u32 tries = 0; tries < 1000000; ++tries) {
        if ((inb(PIT_SPEAKER_PORT) & 0x20u) != 0) {
            return;
        }
    }
}

static void show_boot_screen(void)
{
    u8 speaker_state = inb(PIT_SPEAKER_PORT);
    outb(PIT_SPEAKER_PORT, (u8)((speaker_state | 0x01u) & ~0x02u));
    framebuffer_draw_boot_screen();
    for (u32 step = 0; step < 20; ++step) {
        boot_delay_tick();
        framebuffer_draw_boot_progress(step + 1);
    }
    outb(PIT_SPEAKER_PORT, speaker_state);
}

static void power_restart(void)
{
    __asm__ volatile ("cli" ::: "memory");
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);

    struct __attribute__((packed)) {
        u16 limit;
        u32 base;
    } reset_idt = {0, 0};
    __asm__ volatile ("lidt %0; int3" : : "m"(reset_idt) : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void power_shutdown(void)
{
    __asm__ volatile ("cli" ::: "memory");
    outb(0x604, 0x00);
    outb(0x605, 0x20);
    outb(0xB004, 0x00);
    outb(0xB005, 0x20);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kernel_main(u32 magic, u32 multiboot_info_address)
{
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC ||
        !framebuffer_initialize(multiboot_info_address)) {
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    fat16_initialize();
    show_boot_screen();
    framebuffer_draw_desktop();
    mouse_set_bounds(framebuffer_width(), framebuffer_height());
    mouse_initialize();
    interrupts_initialize();
    interrupts_enable_input_irqs();

    i32 cursor_x;
    i32 cursor_y;
    mouse_get_position(&cursor_x, &cursor_y);
    u8 previous_buttons = mouse_get_buttons();
    u32 last_caret_blink = interrupts_timer_ticks();
    framebuffer_draw_pointer_status(cursor_x, cursor_y);
    framebuffer_xor_cursor(cursor_x, cursor_y);
    interrupts_enable();

    for (;;) {
        __asm__ volatile ("hlt");
        u32 current_ticks = interrupts_timer_ticks();
        if ((u32)(current_ticks - last_caret_blink) >= 25) {
            last_caret_blink = current_ticks;
            framebuffer_xor_cursor(cursor_x, cursor_y);
            framebuffer_blink_caret();
            framebuffer_xor_cursor(cursor_x, cursor_y);
        }
        i32 next_x;
        i32 next_y;
        char typed_character;
        mouse_get_position(&next_x, &next_y);
        u8 buttons = mouse_get_buttons();
        int pointer_moved = next_x != cursor_x || next_y != cursor_y;
        int has_typed_character = keyboard_pop_char(&typed_character);
        if (pointer_moved ||
            buttons != previous_buttons || has_typed_character) {
            framebuffer_xor_cursor(cursor_x, cursor_y);
            cursor_x = next_x;
            cursor_y = next_y;
            if ((buttons & 1u) != 0 && (previous_buttons & 1u) == 0) {
                framebuffer_handle_click(cursor_x, cursor_y);
                u8 power_action = framebuffer_take_power_action();
                if (power_action == POWER_ACTION_RESTART) {
                    power_restart();
                }
                if (power_action == POWER_ACTION_SHUTDOWN) {
                    power_shutdown();
                }
            }
            if (pointer_moved || buttons != previous_buttons) {
                framebuffer_pointer_move(cursor_x, cursor_y, buttons);
            }
            if (has_typed_character) {
                framebuffer_note_input(typed_character);
                framebuffer_file_input(typed_character);
                framebuffer_calculator_input(typed_character);
            }
            framebuffer_draw_pointer_status(cursor_x, cursor_y);
            framebuffer_xor_cursor(cursor_x, cursor_y);
            previous_buttons = buttons;
        }
    }
}
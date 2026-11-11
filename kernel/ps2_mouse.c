#include "include/io.h"
#include "include/mouse.h"

#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_DATA_PORT 0x60

static volatile i32 cursor_x = 400;
static volatile i32 cursor_y = 300;
static volatile u32 screen_width = 800;
static volatile u32 screen_height = 600;
static volatile u8 mouse_buttons;
static u8 packet[3];
static u8 packet_index;

static int wait_input_clear(void)
{
    for (u32 tries = 0; tries < 100000; ++tries) {
        if ((inb(PS2_STATUS_PORT) & 0x02u) == 0) {
            return 1;
        }
    }
    return 0;
}

static int wait_output_full(void)
{
    for (u32 tries = 0; tries < 100000; ++tries) {
        if ((inb(PS2_STATUS_PORT) & 0x01u) != 0) {
            return 1;
        }
    }
    return 0;
}

static int controller_command(u8 command)
{
    if (!wait_input_clear()) {
        return 0;
    }
    outb(PS2_COMMAND_PORT, command);
    return 1;
}

static int mouse_command(u8 command)
{
    if (!controller_command(0xD4) || !wait_input_clear()) {
        return 0;
    }
    outb(PS2_DATA_PORT, command);
    if (!wait_output_full()) {
        return 0;
    }
    return inb(PS2_DATA_PORT) == 0xFA;
}

void mouse_set_bounds(u32 width, u32 height)
{
    screen_width = width;
    screen_height = height;
    cursor_x = (i32)(width / 2);
    cursor_y = (i32)(height / 2);
}

void mouse_get_position(i32 *x, i32 *y)
{
    __asm__ volatile ("cli" ::: "memory");
    *x = cursor_x;
    *y = cursor_y;
    __asm__ volatile ("sti" ::: "memory");
}

u8 mouse_get_buttons(void)
{
    u8 buttons;
    __asm__ volatile ("cli" ::: "memory");
    buttons = mouse_buttons;
    __asm__ volatile ("sti" ::: "memory");
    return buttons;
}

void mouse_initialize(void)
{
    if (!controller_command(0xA8) || !controller_command(0x20) || !wait_output_full()) {
        return;
    }

    u8 config = inb(PS2_DATA_PORT);
    config = (u8)((config | 0x02u) & ~0x20u);
    if (!controller_command(0x60) || !wait_input_clear()) {
        return;
    }
    outb(PS2_DATA_PORT, config);

    (void)mouse_command(0xF6);
    (void)mouse_command(0xF4);
    packet_index = 0;
}

static i32 signed_delta(u8 value, u8 sign_bit)
{
    return (sign_bit != 0) ? (i32)value - 256 : (i32)value;
}

void mouse_handle_irq(void)
{
    u8 status = inb(PS2_STATUS_PORT);
    if ((status & 0x01u) == 0 || (status & 0x20u) == 0) {
        return;
    }

    u8 value = inb(PS2_DATA_PORT);
    if (packet_index == 0 && (value & 0x08u) == 0) {
        return;
    }
    packet[packet_index++] = value;
    if (packet_index < 3) {
        return;
    }
    packet_index = 0;

    if ((packet[0] & 0xC0u) != 0) {
        return;
    }

    mouse_buttons = packet[0] & 0x07u;
    i32 next_x = cursor_x + signed_delta(packet[1], packet[0] & 0x10u);
    i32 next_y = cursor_y - signed_delta(packet[2], packet[0] & 0x20u);
    if (next_x < 0) {
        next_x = 0;
    } else if ((u32)next_x >= screen_width) {
        next_x = (i32)screen_width - 1;
    }
    if (next_y < 0) {
        next_y = 0;
    } else if ((u32)next_y >= screen_height) {
        next_y = (i32)screen_height - 1;
    }
    cursor_x = next_x;
    cursor_y = next_y;
}
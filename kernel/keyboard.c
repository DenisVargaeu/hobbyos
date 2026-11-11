#include "include/io.h"
#include "include/keyboard.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEY_QUEUE_SIZE 64

static volatile char key_queue[KEY_QUEUE_SIZE];
static volatile u8 queue_read;
static volatile u8 queue_write;
static u8 shift_down;

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0E] = '\b', [0x10] = 'q',
    [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o',
    [0x19] = 'p', [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's',
    [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x2C] = 'z',
    [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x39] = ' '
};

int keyboard_pop_char(char *character)
{
    __asm__ volatile ("cli" ::: "memory");
    if (queue_read == queue_write) {
        __asm__ volatile ("sti" ::: "memory");
        return 0;
    }

    *character = key_queue[queue_read];
    queue_read = (u8)((queue_read + 1u) % KEY_QUEUE_SIZE);
    __asm__ volatile ("sti" ::: "memory");
    return 1;
}

void keyboard_irq_handler(void)
{
    u8 scancode = inb(KEYBOARD_DATA_PORT);
    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = 0;
    } else if ((scancode & 0x80u) == 0 && scancode < 128) {
        char character = keymap[scancode];
        if (character != '\0') {
            if (shift_down && character >= 'a' && character <= 'z') {
                character = (char)(character - 'a' + 'A');
            }
            u8 next_write = (u8)((queue_write + 1u) % KEY_QUEUE_SIZE);
            if (next_write != queue_read) {
                key_queue[queue_write] = character;
                queue_write = next_write;
            }
        }
    }

    outb(0x20, 0x20);
}
#include "include/interrupts.h"
#include "include/io.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/types.h"

typedef struct __attribute__((packed)) {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 attributes;
    u16 offset_middle;
    u32 offset_high;
    u32 reserved;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} idt_pointer_t;

static idt_entry_t idt[256];

extern void mouse_irq_stub(void);
extern void keyboard_irq_stub(void);

static void set_idt_gate(u8 vector, void (*handler)(void))
{
    u64 address = (u64)handler;
    u16 code_selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_selector));
    idt[vector].offset_low = (u16)(address & 0xFFFFu);
    idt[vector].selector = code_selector;
    idt[vector].ist = 0;
    idt[vector].attributes = 0x8E;
    idt[vector].offset_middle = (u16)(address >> 16);
    idt[vector].offset_high = (u32)(address >> 32);
    idt[vector].reserved = 0;
}

static void remap_pic(void)
{
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void interrupts_initialize(void)
{
    set_idt_gate(0x21, keyboard_irq_stub);
    set_idt_gate(0x2C, mouse_irq_stub);
    idt_pointer_t pointer = {
        .limit = (u16)(sizeof(idt) - 1),
        .base = (u64)idt
    };
    __asm__ volatile ("lidt %0" : : "m"(pointer));
    remap_pic();
}

void interrupts_enable_input_irqs(void)
{
    outb(0x21, (u8)(inb(0x21) & ~0x06u));
    outb(0xA1, (u8)(inb(0xA1) & ~0x10u));
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti" ::: "memory");
}

void mouse_irq_handler(void)
{
    mouse_handle_irq();
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

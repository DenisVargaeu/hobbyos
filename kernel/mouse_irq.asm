BITS 32

section .text
global mouse_irq_stub
extern mouse_irq_handler

mouse_irq_stub:
    pushad
    cld
    call mouse_irq_handler
    popad
    iretd
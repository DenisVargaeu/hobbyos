BITS 32

section .text
global keyboard_irq_stub
extern keyboard_irq_handler

keyboard_irq_stub:
    pushad
    cld
    call keyboard_irq_handler
    popad
    iretd
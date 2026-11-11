BITS 32

section .multiboot2
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + (header_end - header_start))

    align 8
    dw 5
    dw 0
    dd 20
    dd 800
    dd 600
    dd 32

    align 8
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    cld
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt
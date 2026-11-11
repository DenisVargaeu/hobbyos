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

align 4096
pml4:
    resq 512
pdpt:
    resq 512
page_directories:
    resq 2048

section .data
align 4
boot_magic:
    dd 0
boot_info:
    dd 0
info_end:
    dd 0
module_start:
    dd 0
program_header:
    dd 0
program_header_count:
    dd 0
program_header_size:
    dd 0
kernel_entry:
    dq 0

section .text
global _start

_start:
    cli
    cld
    mov esp, stack_top
    mov [boot_magic], eax
    mov [boot_info], ebx
    mov eax, [ebx]
    add eax, ebx
    mov [info_end], eax

    mov esi, ebx
    add esi, 8
.find_tag:
    cmp esi, [info_end]
    jae .halt
    mov eax, [esi]
    mov edx, [esi + 4]
    cmp eax, 0
    je .halt
    cmp eax, 3
    je .module_found
    add edx, 7
    and edx, 0xFFFFFFF8
    add esi, edx
    jmp .find_tag

.module_found:
    cmp edx, 16
    jb .halt
    mov eax, [esi + 8]
    mov [module_start], eax
    mov ebp, eax
    cmp byte [ebp + 4], 2
    jne .halt
    cmp word [ebp + 18], 62
    jne .halt

    mov eax, [ebp + 24]
    mov [kernel_entry], eax
    mov eax, [ebp + 32]
    add eax, ebp
    mov [program_header], eax
    movzx eax, word [ebp + 54]
    mov [program_header_size], eax
    movzx eax, word [ebp + 56]
    mov [program_header_count], eax

.load_segment:
    cmp dword [program_header_count], 0
    je .segments_loaded
    mov ebx, [program_header]
    cmp dword [ebx], 1
    jne .next_segment

    mov edi, [ebx + 24]
    mov esi, [module_start]
    add esi, [ebx + 8]
    mov ecx, [ebx + 32]
    rep movsb

    mov ebx, [program_header]
    mov ecx, [ebx + 40]
    sub ecx, [ebx + 32]
    xor eax, eax
    rep stosb

.next_segment:
    mov eax, [program_header]
    add eax, [program_header_size]
    mov [program_header], eax
    dec dword [program_header_count]
    jmp .load_segment

.segments_loaded:
    mov edi, pml4
    mov eax, pdpt
    or eax, 3
    mov [edi], eax

    mov edi, pdpt
    mov eax, page_directories
    mov ecx, 4
.map_pdpt:
    mov edx, eax
    or edx, 3
    mov [edi], edx
    add edi, 8
    add eax, 4096
    loop .map_pdpt

    mov edi, page_directories
    mov eax, 0x83
    mov ecx, 2048
.map_pages:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .map_pages

    lgdt [gdt_pointer]
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax
    jmp 0x08:long_mode_start

.halt:
    cli
    hlt
    jmp .halt

BITS 64
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov rsp, stack_top
    and rsp, -16

    mov edi, [rel boot_magic]
    mov esi, [rel boot_info]
    mov rax, [rel kernel_entry]
    call rax

.halt64:
    cli
    hlt
    jmp .halt64

align 8
gdt_start:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_pointer:
    dw gdt_end - gdt_start - 1
    dd gdt_start
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 16M;

    .text ALIGN(4K) :
    {
        *(.text*)
    } :text

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    } :text

    .data ALIGN(4K) :
    {
        *(.data*)
    } :data

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
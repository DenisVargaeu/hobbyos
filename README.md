# Hobby OS

A small 32-bit x86 hobby OS with a Multiboot2 entry point, a C framebuffer UI,
and PS/2 mouse input. GRUB loads the ELF kernel and supplies an RGB framebuffer;
the kernel draws a simple desktop and moves a software cursor from mouse IRQs.

## Requirements

- GCC with 32-bit freestanding compilation support
- NASM
- GNU binutils
- GRUB utilities (`grub-mkrescue`)
- xorriso
- QEMU system emulator for x86

## Build and run

```sh
make
make run
```

The bootable ISO is written to `build/hobby-os.iso`. `make clean` removes build
outputs. QEMU's default PC machine provides the emulated PS/2 mouse; hovering
over the emulator window captures pointer input and moves the on-screen cursor.

## Layout

- `boot/entry.asm`: Multiboot2 header and 32-bit kernel entry/stack
- `kernel/`: freestanding C kernel, framebuffer, PIC/IDT, and mouse driver
- `linker.ld`: kernel ELF layout, keeping the Multiboot2 header near the start
- `grub/grub.cfg`: ISO boot menu and kernel load command
- `Makefile`: ELF, ISO, run, and clean targets
# Nova OS

A small x86-64 hobby operating system. GRUB loads a Multiboot2 ELF32 bootstrap,
which loads the ELF64 kernel, creates identity mappings for the first 4 GiB,
and switches the processor into long mode. The freestanding C kernel draws a
graphical desktop using the RGB framebuffer supplied by GRUB.

The desktop includes a boot splash, mouse-driven app drawer, Notes editor,
four-operation calculator, Files/About/Terminal panels, and a Power menu with
restart and shutdown actions. The white software cursor is controlled by the
emulated PS/2 mouse. Keyboard input and mouse input use separate IRQ handlers.

## Requirements

- GCC with x86-64 freestanding compilation support and 32-bit linker support
- NASM
- GNU binutils
- GRUB utilities (`grub-mkrescue`)
- mtools (`mformat`, used by GRUB to make the EFI image)
- xorriso
- QEMU system emulator for x86

On Arch Linux, install the packages with:

```sh
sudo pacman -S base-devel nasm grub xorriso mtools qemu-system-x86
```

## Build and run

```sh
make
make run
```

The bootable ISO is written to `build/hobby-os.iso`. `make clean` removes build
outputs. QEMU's GTK window captures the pointer on hover. Open **APPS** to
launch an app; in Notes, type normally. In Calculator, click the keypad or use
number keys, `+`, `-`, `*`, `/`, `=`, and Backspace. **POWER** opens the restart
and shutdown actions. Click the close control in an app window to return to the
desktop.

## Layout

- `boot/entry.asm`: Multiboot2 ELF32 bootstrap, ELF64 loader, and long-mode switch
- `boot/linker32.ld`: physical layout for the temporary bootstrap
- `kernel/`: freestanding C kernel, framebuffer UI, PIC/IDT, PS/2 mouse, and
  keyboard drivers
- `linker.ld`: ELF64 kernel load segments
- `grub/grub.cfg`: ISO boot menu and kernel load command
- `Makefile`: ELF, ISO, run, and clean targets
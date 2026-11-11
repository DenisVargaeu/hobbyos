# Nova OS

A small 32-bit x86 hobby operating system. NASM provides the Multiboot2 entry
point, and a freestanding C kernel draws a graphical desktop using the RGB
framebuffer supplied by GRUB.

The desktop includes a boot splash, mouse-driven app drawer, Notes editor,
four-operation calculator, Files/About/Terminal panels, and a Power menu with
restart and shutdown actions. The white software cursor is controlled by the
emulated PS/2 mouse. Keyboard input and mouse input use separate IRQ handlers.

## Requirements

- GCC with 32-bit freestanding compilation support
- NASM
- GNU binutils
- GRUB utilities (`grub-mkrescue`)
- mtools (`mformat`, used by GRUB to make the EFI image)
- xorriso
- QEMU system emulator for x86

On Arch Linux, install the packages with:

```sh
sudo pacman -S base-devel nasm grub xorriso mtools qemu-system-i386
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

- `boot/entry.asm`: Multiboot2 header and 32-bit kernel entry/stack
- `kernel/`: freestanding C kernel, framebuffer UI, PIC/IDT, PS/2 mouse, and
  keyboard drivers
- `linker.ld`: kernel ELF layout, keeping the Multiboot2 header near the start
- `grub/grub.cfg`: ISO boot menu and kernel load command
- `Makefile`: ELF, ISO, run, and clean targets
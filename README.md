# Nova OS

A small x86-64 hobby operating system. GRUB loads a Multiboot2 ELF32 bootstrap,
which loads the ELF64 kernel, creates identity mappings for the first 4 GiB,
and switches the processor into long mode. The freestanding C kernel draws a
graphical desktop using the RGB framebuffer supplied by GRUB.

Current version: **1.10.0**. The version is shown on the boot splash and in About OS.

The desktop includes a boot splash, navbar, app drawer, taskbar, and a basic
window manager. Changes are composed in a RAM backbuffer and only the finished
dirty region is copied to the display, avoiding visible partial redraws while
moving, focusing, opening, or closing windows. Files,
Notes, Calculator, Terminal, and About OS can stay open in separate windows at
the same time. Drag a window by its **MOVE** titlebar area to reposition it; use its minus button
to minimize it, or its X button to close it.
Notes and Calculator receive keyboard input when focused; the calculator also
has a clickable keypad. The Power menu has restart and shutdown actions. The
white software cursor is controlled by the emulated PS/2 mouse.

Files uses a FAT16 volume on a persistent 16 MiB IDE disk image. On first use,
choose **MOUNT**; an unformatted disk then offers **FORMAT**. After formatting,
choose **MOUNT** again. The FAT16 root supports up to 32 files with 8.3 names;
the editor supports files up to 511 text bytes. Use **NEW** to create a file,
select it to edit, and type; **RENAME** edits the 8.3 filename and Enter saves
it. Shift and Caps Lock switch letter case, and lowercase and uppercase render
distinctly. Backspace removes the previous character and Enter starts a new
line. A blinking white caret marks the insertion point while editing.
Changes are written to disk immediately and survive OS restarts. The image is
`build/nova-storage.img`; `make clean` removes it, so copy it elsewhere first
if you need to preserve its contents before cleaning the build directory.

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
sudo pacman -S base-devel nasm grub xorriso mtools qemu-img qemu-system-x86
```

## Build and run

```sh
make
make run
```

The bootable ISO is written to `build/hobby-os.iso`. `make clean` removes build
outputs. QEMU's GTK window captures the pointer on hover. Use **APPS** to launch
several applications; their taskbar buttons switch between or minimize windows.
In Calculator, use the on-screen keypad or number keys, `+`, `-`, `*`, `/`, `=`,
and Backspace. **POWER** opens the restart and shutdown actions.

## Layout

- `boot/entry.asm`: Multiboot2 ELF32 bootstrap, ELF64 loader, and long-mode switch
- `boot/linker32.ld`: physical layout for the temporary bootstrap
- `kernel/`: freestanding C kernel, framebuffer UI, PIC/IDT, PS/2 mouse, and
  keyboard drivers
- `linker.ld`: ELF64 kernel load segments
- `grub/grub.cfg`: ISO boot menu and kernel load command
- `Makefile`: ELF, ISO, run, and clean targets
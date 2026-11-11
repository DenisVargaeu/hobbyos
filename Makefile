BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/iso
BOOTSTRAP := $(BUILD_DIR)/boot32.elf
KERNEL := $(BUILD_DIR)/kernel64.elf
ISO := $(BUILD_DIR)/hobby-os.iso
VERSION := 1.6.0

CC := gcc
LD := ld
NASM := nasm
CFLAGS := -m64 -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding \
          -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
		  -mno-red-zone -mgeneral-regs-only -mcmodel=small \
		  -DNOVA_VERSION='"$(VERSION)"' -Ikernel
C_SOURCES := kernel/main.c kernel/framebuffer.c kernel/interrupts.c kernel/ps2_mouse.c kernel/keyboard.c kernel/ramfs.c
ASM_SOURCES := kernel/mouse_irq.asm kernel/keyboard_irq.asm
C_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
BOOT_OBJECT := $(BUILD_DIR)/boot/entry32.o

.PHONY: all iso run clean

all: iso

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: kernel/%.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BOOT_OBJECT): boot/entry.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(BOOTSTRAP): $(BOOT_OBJECT) boot/linker32.ld
	$(LD) -m elf_i386 -T boot/linker32.ld -nostdlib -o $@ $(BOOT_OBJECT)

$(KERNEL): $(C_OBJECTS) $(ASM_OBJECTS) linker.ld
	$(LD) -m elf_x86_64 -z max-page-size=0x1000 -T linker.ld -nostdlib -o $@ $(C_OBJECTS) $(ASM_OBJECTS)

$(ISO): $(BOOTSTRAP) $(KERNEL) grub/grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	cp $(BOOTSTRAP) $(ISO_ROOT)/boot/boot32.elf
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel64.elf
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_ROOT)

iso: $(ISO)

run: $(ISO)
	qemu-system-x86_64 -m 128M -cdrom $(ISO) -boot d -vga std \
		-display gtk,grab-on-hover=on,show-cursor=off

clean:
	rm -rf $(BUILD_DIR)
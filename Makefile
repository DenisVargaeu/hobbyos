BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.elf
ISO := $(BUILD_DIR)/hobby-os.iso

CC := gcc
LD := ld
NASM := nasm
CFLAGS := -m32 -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding \
          -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mno-sse -mno-mmx -msoft-float -Ikernel
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
C_SOURCES := kernel/main.c kernel/framebuffer.c kernel/interrupts.c kernel/ps2_mouse.c kernel/keyboard.c
ASM_SOURCES := boot/entry.asm kernel/mouse_irq.asm kernel/keyboard_irq.asm
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
           $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

.PHONY: all iso run clean

all: iso

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(ISO): $(KERNEL) grub/grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_ROOT)

iso: $(ISO)

run: $(ISO)
	qemu-system-i386 -m 128M -cdrom $(ISO) -boot d -vga std \
		-display gtk,grab-on-hover=on,show-cursor=off

clean:
	rm -rf $(BUILD_DIR)
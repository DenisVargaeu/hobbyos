#include "include/ata.h"
#include "include/io.h"

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7
#define ATA_ALT_STATUS 0x3F6

#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_BSY 0x80

static u32 total_sectors;

static void select_drive(u32 lba)
{
    outb(ATA_DRIVE, (u8)(0xE0u | ((lba >> 24) & 0x0Fu)));
    (void)inb(ATA_ALT_STATUS);
    (void)inb(ATA_ALT_STATUS);
    (void)inb(ATA_ALT_STATUS);
    (void)inb(ATA_ALT_STATUS);
}

static int wait_status(int require_data)
{
    for (u32 tries = 0; tries < 1000000; ++tries) {
        u8 status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF || (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
            return 0;
        }
        if ((status & ATA_STATUS_BSY) == 0 &&
            (!require_data || (status & ATA_STATUS_DRQ) != 0)) {
            return 1;
        }
    }
    return 0;
}

int ata_initialize(void)
{
    total_sectors = 0;
    outb(ATA_DRIVE, 0xA0);
    io_wait();
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_COMMAND, 0xEC);

    u8 status = inb(ATA_STATUS);
    if (status == 0 || status == 0xFF || !wait_status(1)) {
        return 0;
    }

    u16 identify[256];
    for (u32 index = 0; index < 256; ++index) {
        identify[index] = inw(ATA_DATA);
    }
    if ((identify[49] & (1u << 9)) == 0) {
        return 0;
    }
    total_sectors = (u32)identify[60] | ((u32)identify[61] << 16);
    return total_sectors != 0;
}

u32 ata_sector_count(void)
{
    return total_sectors;
}

static int begin_sector_command(u32 lba, u8 command)
{
    if (lba >= total_sectors || lba >= 0x10000000u) {
        return 0;
    }
    select_drive(lba);
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA0, (u8)lba);
    outb(ATA_LBA1, (u8)(lba >> 8));
    outb(ATA_LBA2, (u8)(lba >> 16));
    outb(ATA_COMMAND, command);
    return 1;
}

int ata_read_sector(u32 lba, u8 *buffer)
{
    if (buffer == NULL || !begin_sector_command(lba, 0x20) || !wait_status(1)) {
        return 0;
    }
    for (u32 index = 0; index < 256; ++index) {
        u16 word = inw(ATA_DATA);
        buffer[index * 2] = (u8)word;
        buffer[index * 2 + 1] = (u8)(word >> 8);
    }
    return wait_status(0);
}

int ata_write_sector(u32 lba, const u8 *buffer)
{
    if (buffer == NULL || !begin_sector_command(lba, 0x30) || !wait_status(1)) {
        return 0;
    }
    for (u32 index = 0; index < 256; ++index) {
        u16 word = (u16)buffer[index * 2] |
                   ((u16)buffer[index * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    if (!wait_status(0)) {
        return 0;
    }
    outb(ATA_COMMAND, 0xE7);
    return wait_status(0);
}
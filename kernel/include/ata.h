#ifndef ATA_H
#define ATA_H

#include "types.h"

int ata_initialize(void);
u32 ata_sector_count(void);
int ata_read_sector(u32 lba, u8 *buffer);
int ata_write_sector(u32 lba, const u8 *buffer);

#endif
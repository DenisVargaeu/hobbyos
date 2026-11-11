#ifndef FAT16_H
#define FAT16_H

#include "types.h"

#define FAT16_MAX_FILES 32
#define FAT16_NAME_SIZE 13
#define FAT16_DATA_SIZE 512

typedef struct {
    char name[FAT16_NAME_SIZE];
    char data[FAT16_DATA_SIZE];
    u32 size;
    u32 directory_sector;
    u16 first_cluster;
    u8 directory_offset;
    u8 used;
} fat16_file_t;

void fat16_initialize(void);
int fat16_disk_present(void);
int fat16_is_mounted(void);
int fat16_needs_format(void);
int fat16_mount(void);
int fat16_unmount(void);
int fat16_format(void);
u32 fat16_file_count(void);
fat16_file_t *fat16_file_at(u32 index);
int fat16_create(const char *name);
int fat16_rename(u32 index, const char *name);
int fat16_append(u32 index, char character);
int fat16_backspace(u32 index);

#endif
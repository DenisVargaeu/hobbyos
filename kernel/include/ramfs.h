#ifndef RAMFS_H
#define RAMFS_H

#include "types.h"

#define RAMFS_MAX_FILES 8
#define RAMFS_NAME_SIZE 16
#define RAMFS_DATA_SIZE 512

typedef struct {
    char name[RAMFS_NAME_SIZE];
    char data[RAMFS_DATA_SIZE];
    u32 size;
    u8 used;
} ramfs_file_t;

void ramfs_initialize(void);
u32 ramfs_count(void);
ramfs_file_t *ramfs_file_at(u32 index);
int ramfs_create(const char *name);
int ramfs_append(u32 index, char character);
int ramfs_backspace(u32 index);

#endif
#include "include/ramfs.h"

static ramfs_file_t files[RAMFS_MAX_FILES];
static u32 file_count;

static void copy_text(char *destination, const char *source, u32 capacity)
{
    u32 index = 0;
    while (index + 1 < capacity && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static int create_with_content(const char *name, const char *content)
{
    if (file_count >= RAMFS_MAX_FILES) {
        return -1;
    }

    u32 index = file_count++;
    ramfs_file_t *file = &files[index];
    copy_text(file->name, name, sizeof(file->name));
    copy_text(file->data, content, sizeof(file->data));
    file->size = 0;
    while (file->data[file->size] != '\0') {
        ++file->size;
    }
    file->used = 1;
    return (int)index;
}

void ramfs_initialize(void)
{
    for (u32 index = 0; index < RAMFS_MAX_FILES; ++index) {
        files[index].used = 0;
        files[index].size = 0;
    }
    file_count = 0;
    create_with_content("WELCOME.TXT", "NOVA OS RAM FILE SYSTEM\nEDIT THIS FILE OR CREATE A NEW ONE.");
    create_with_content("NOTES.TXT", "");
}

u32 ramfs_count(void)
{
    return file_count;
}

ramfs_file_t *ramfs_file_at(u32 index)
{
    if (index >= file_count || !files[index].used) {
        return NULL;
    }
    return &files[index];
}

int ramfs_create(const char *name)
{
    return create_with_content(name, "");
}

int ramfs_append(u32 index, char character)
{
    ramfs_file_t *file = ramfs_file_at(index);
    if (file == NULL || file->size + 1 >= RAMFS_DATA_SIZE) {
        return 0;
    }
    file->data[file->size++] = character;
    file->data[file->size] = '\0';
    return 1;
}

int ramfs_backspace(u32 index)
{
    ramfs_file_t *file = ramfs_file_at(index);
    if (file == NULL || file->size == 0) {
        return 0;
    }
    file->data[--file->size] = '\0';
    return 1;
}
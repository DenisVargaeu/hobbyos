#include "include/ata.h"
#include "include/fat16.h"

#define SECTOR_SIZE 512u
#define FAT16_EOC 0xFFFFu
#define FAT16_MIN_CLUSTERS 4085u
#define FAT16_MAX_CLUSTERS 65524u

static fat16_file_t files[FAT16_MAX_FILES];
static u32 file_count;
static u32 volume_start;
static u32 volume_sectors;
static u32 fat_start;
static u32 fat_sectors;
static u32 root_start;
static u32 root_sectors;
static u32 data_start;
static u32 cluster_count;
static u16 root_entries;
static u8 sectors_per_cluster;
static u8 fat_count;
static int disk_present;
static int mounted;
static int format_needed;

static u16 get_u16(const u8 *bytes)
{
    return (u16)bytes[0] | ((u16)bytes[1] << 8);
}

static u32 get_u32(const u8 *bytes)
{
    return (u32)bytes[0] | ((u32)bytes[1] << 8) |
           ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static void put_u16(u8 *bytes, u16 value)
{
    bytes[0] = (u8)value;
    bytes[1] = (u8)(value >> 8);
}

static void put_u32(u8 *bytes, u32 value)
{
    bytes[0] = (u8)value;
    bytes[1] = (u8)(value >> 8);
    bytes[2] = (u8)(value >> 16);
    bytes[3] = (u8)(value >> 24);
}

static int read_volume_sector(u32 sector, u8 *buffer)
{
    if (sector >= volume_sectors || volume_start + sector < volume_start) {
        return 0;
    }
    return ata_read_sector(volume_start + sector, buffer);
}

static int write_volume_sector(u32 sector, const u8 *buffer)
{
    if (sector >= volume_sectors || volume_start + sector < volume_start) {
        return 0;
    }
    return ata_write_sector(volume_start + sector, buffer);
}

static int fat16_boot_sector_valid(const u8 *boot, u32 available_sectors)
{
    if (boot[510] != 0x55 || boot[511] != 0xAA ||
        get_u16(&boot[11]) != SECTOR_SIZE ||
        boot[13] == 0 || (boot[13] & (boot[13] - 1u)) != 0 ||
        get_u16(&boot[14]) == 0 || boot[16] == 0 ||
        get_u16(&boot[17]) == 0 || get_u16(&boot[22]) == 0) {
        return 0;
    }

    u32 total = get_u16(&boot[19]);
    if (total == 0) {
        total = get_u32(&boot[32]);
    }
    if (total == 0 || total > available_sectors ||
        (u32)get_u16(&boot[22]) * boot[16] >= total) {
        return 0;
    }

    u32 root_size = ((u32)get_u16(&boot[17]) * 32u + SECTOR_SIZE - 1u) / SECTOR_SIZE;
    u32 first_data = get_u16(&boot[14]) +
                     (u32)boot[16] * get_u16(&boot[22]) + root_size;
    if (first_data >= total) {
        return 0;
    }
    u32 clusters = (total - first_data) / boot[13];
    return clusters >= FAT16_MIN_CLUSTERS && clusters <= FAT16_MAX_CLUSTERS;
}

static int mount_volume(u32 start, u32 available_sectors, const u8 *boot)
{
    if (!fat16_boot_sector_valid(boot, available_sectors)) {
        return 0;
    }

    volume_start = start;
    volume_sectors = get_u16(&boot[19]);
    if (volume_sectors == 0) {
        volume_sectors = get_u32(&boot[32]);
    }
    sectors_per_cluster = boot[13];
    fat_count = boot[16];
    root_entries = get_u16(&boot[17]);
    fat_sectors = get_u16(&boot[22]);
    root_sectors = ((u32)root_entries * 32u + SECTOR_SIZE - 1u) / SECTOR_SIZE;
    fat_start = get_u16(&boot[14]);
    root_start = fat_start + (u32)fat_count * fat_sectors;
    data_start = root_start + root_sectors;
    cluster_count = (volume_sectors - data_start) / sectors_per_cluster;
    mounted = 1;
    format_needed = 0;
    return 1;
}

static u16 read_fat_entry(u16 cluster)
{
    u8 sector[SECTOR_SIZE];
    u32 offset = (u32)cluster * 2u;
    if (!read_volume_sector(fat_start + offset / SECTOR_SIZE, sector)) {
        return 0;
    }
    return get_u16(&sector[offset % SECTOR_SIZE]);
}

static int write_fat_entry(u16 cluster, u16 value)
{
    u32 offset = (u32)cluster * 2u;
    u32 sector_index = offset / SECTOR_SIZE;
    for (u32 copy = 0; copy < fat_count; ++copy) {
        u8 sector[SECTOR_SIZE];
        u32 sector_number = fat_start + copy * fat_sectors + sector_index;
        if (!read_volume_sector(sector_number, sector)) {
            return 0;
        }
        put_u16(&sector[offset % SECTOR_SIZE], value);
        if (!write_volume_sector(sector_number, sector)) {
            return 0;
        }
    }
    return 1;
}

static u16 allocate_cluster(void)
{
    for (u32 cluster = 2; cluster < cluster_count + 2; ++cluster) {
        if (read_fat_entry((u16)cluster) == 0 &&
            write_fat_entry((u16)cluster, FAT16_EOC)) {
            return (u16)cluster;
        }
    }
    return 0;
}

static void decode_name(const u8 *entry, char *name)
{
    u32 output = 0;
    for (u32 index = 0; index < 8 && entry[index] != ' '; ++index) {
        name[output++] = (char)entry[index];
    }
    if (entry[8] != ' ') {
        name[output++] = '.';
        for (u32 index = 8; index < 11 && entry[index] != ' '; ++index) {
            name[output++] = (char)entry[index];
        }
    }
    name[output] = '\0';
}

static void load_file_data(fat16_file_t *file)
{
    file->data[0] = '\0';
    if (file->size == 0 || file->first_cluster < 2) {
        return;
    }

    u32 wanted = file->size;
    if (wanted >= FAT16_DATA_SIZE) {
        wanted = FAT16_DATA_SIZE - 1;
    }
    u32 copied = 0;
    u16 cluster = file->first_cluster;
    while (copied < wanted &&
           cluster >= 2 && cluster < cluster_count + 2) {
        u32 first_sector = data_start + (u32)(cluster - 2) * sectors_per_cluster;
        for (u32 offset = 0; offset < sectors_per_cluster && copied < wanted; ++offset) {
            u8 sector[SECTOR_SIZE];
            if (!read_volume_sector(first_sector + offset, sector)) {
                file->size = copied;
                file->data[copied] = '\0';
                return;
            }
            u32 amount = wanted - copied;
            if (amount > SECTOR_SIZE) amount = SECTOR_SIZE;
            if (amount > FAT16_DATA_SIZE - copied) amount = FAT16_DATA_SIZE - copied;
            for (u32 byte = 0; byte < amount; ++byte) {
                file->data[copied + byte] = (char)sector[byte];
            }
            copied += amount;
        }
        if (copied < wanted) {
            cluster = read_fat_entry(cluster);
        }
    }
    file->size = copied;
    file->data[copied] = '\0';
}

static void load_root_directory(void)
{
    file_count = 0;
    for (u32 sector_index = 0; sector_index < root_sectors; ++sector_index) {
        u8 sector[SECTOR_SIZE];
        if (!read_volume_sector(root_start + sector_index, sector)) {
            return;
        }
        for (u32 offset = 0; offset < SECTOR_SIZE; offset += 32) {
            u8 *entry = &sector[offset];
            if (entry[0] == 0) {
                return;
            }
            if (entry[0] == 0xE5 || entry[11] == 0x0F ||
                (entry[11] & 0x18u) != 0 || file_count >= FAT16_MAX_FILES) {
                continue;
            }
            fat16_file_t *file = &files[file_count++];
            decode_name(entry, file->name);
            file->first_cluster = get_u16(&entry[26]);
            file->size = get_u32(&entry[28]);
            file->directory_sector = root_start + sector_index;
            file->directory_offset = (u8)offset;
            file->used = 1;
            load_file_data(file);
        }
    }
}

void fat16_initialize(void)
{
    mounted = 0;
    format_needed = 0;
    file_count = 0;
    disk_present = ata_initialize();
}

int fat16_disk_present(void)
{
    return disk_present;
}

int fat16_is_mounted(void)
{
    return mounted;
}

int fat16_needs_format(void)
{
    return format_needed;
}

int fat16_mount(void)
{
    if (!disk_present) {
        return -1;
    }
    mounted = 0;

    u8 mbr[SECTOR_SIZE];
    u8 boot[SECTOR_SIZE];
    for (u32 index = 0; index < SECTOR_SIZE; ++index) {
        mbr[index] = 0;
        boot[index] = 0;
    }
    if (!ata_read_sector(0, mbr)) {
        format_needed = 1;
        return 0;
    }
    if (fat16_boot_sector_valid(mbr, ata_sector_count()) &&
        mount_volume(0, ata_sector_count(), mbr)) {
        load_root_directory();
        return 1;
    }

    if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
        for (u32 index = 0; index < 4; ++index) {
            u8 *partition = &mbr[446 + index * 16];
            u8 type = partition[4];
            if (type != 0x04 && type != 0x06 && type != 0x0E) {
                continue;
            }
            u32 start = get_u32(&partition[8]);
            u32 length = get_u32(&partition[12]);
            if (length == 0 || start >= ata_sector_count() ||
                !ata_read_sector(start, boot) ||
                !fat16_boot_sector_valid(boot, length)) {
                continue;
            }
            if (mount_volume(start, length, boot)) {
                load_root_directory();
                return 1;
            }
        }
    }

    format_needed = 1;
    return 0;
}

int fat16_unmount(void)
{
    if (!mounted) {
        return 0;
    }
    mounted = 0;
    file_count = 0;
    return 1;
}

int fat16_format(void)
{
    if (!disk_present || ata_sector_count() < 8192) {
        return 0;
    }

    u32 total = ata_sector_count();
    u32 root_count = 512;
    u32 root_size = (root_count * 32u) / SECTOR_SIZE;
    u32 fat_size = 1;
    for (u32 attempt = 0; attempt < 16; ++attempt) {
        if (total <= 1 + root_size + 2 * fat_size) {
            return 0;
        }
        u32 clusters = total - 1 - root_size - 2 * fat_size;
        u32 required = ((clusters + 2) * 2u + SECTOR_SIZE - 1u) / SECTOR_SIZE;
        if (required == fat_size) {
            break;
        }
        fat_size = required;
    }

    u32 clusters = total - 1 - root_size - 2 * fat_size;
    if (clusters < FAT16_MIN_CLUSTERS || clusters > FAT16_MAX_CLUSTERS || fat_size > 0xFFFFu) {
        return 0;
    }

    u8 sector[SECTOR_SIZE];
    for (u32 index = 0; index < SECTOR_SIZE; ++index) sector[index] = 0;
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;
    const char oem[8] = {'N','O','V','A','F','S',' ',' '};
    for (u32 index = 0; index < 8; ++index) sector[3 + index] = (u8)oem[index];
    put_u16(&sector[11], SECTOR_SIZE);
    sector[13] = 1;
    put_u16(&sector[14], 1);
    sector[16] = 2;
    put_u16(&sector[17], (u16)root_count);
    if (total <= 0xFFFFu) {
        put_u16(&sector[19], (u16)total);
    } else {
        put_u32(&sector[32], total);
    }
    sector[21] = 0xF8;
    put_u16(&sector[22], (u16)fat_size);
    put_u16(&sector[24], 63);
    put_u16(&sector[26], 16);
    sector[36] = 0x80;
    sector[38] = 0x29;
    put_u32(&sector[39], 0x4E4F5641u);
    const char label[11] = {'N','O','V','A',' ','D','I','S','K',' ',' '};
    for (u32 index = 0; index < 11; ++index) sector[43 + index] = (u8)label[index];
    const char type[8] = {'F','A','T','1','6',' ',' ',' '};
    for (u32 index = 0; index < 8; ++index) sector[54 + index] = (u8)type[index];
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!ata_write_sector(0, sector)) {
        return 0;
    }

    for (u32 index = 0; index < SECTOR_SIZE; ++index) sector[index] = 0;
    for (u32 copy = 0; copy < 2; ++copy) {
        for (u32 fat_sector = 0; fat_sector < fat_size; ++fat_sector) {
            for (u32 index = 0; index < SECTOR_SIZE; ++index) sector[index] = 0;
            if (fat_sector == 0) {
                put_u16(&sector[0], 0xFFF8);
                put_u16(&sector[2], FAT16_EOC);
            }
            if (!ata_write_sector(1 + copy * fat_size + fat_sector, sector)) {
                return 0;
            }
        }
    }

    u32 root_start_sector = 1 + 2 * fat_size;
    for (u32 index = 0; index < SECTOR_SIZE; ++index) sector[index] = 0;
    for (u32 root_sector = 0; root_sector < root_size; ++root_sector) {
        if (!ata_write_sector(root_start_sector + root_sector, sector)) {
            return 0;
        }
    }
    mounted = 0;
    format_needed = 0;
    return 1;
}

u32 fat16_file_count(void)
{
    return mounted ? file_count : 0;
}

fat16_file_t *fat16_file_at(u32 index)
{
    if (!mounted || index >= file_count || !files[index].used) {
        return NULL;
    }
    return &files[index];
}

static int encode_name(const char *name, u8 *entry)
{
    for (u32 index = 0; index < 11; ++index) entry[index] = ' ';
    u32 base = 0;
    u32 extension = 0;
    int after_dot = 0;
    for (u32 index = 0; name[index] != '\0'; ++index) {
        char character = name[index];
        if (character == '.') {
            if (after_dot) return 0;
            after_dot = 1;
            continue;
        }
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' || character == '-')) {
            return 0;
        }
        if (!after_dot) {
            if (base >= 8) return 0;
            entry[base++] = (u8)character;
        } else {
            if (extension >= 3) return 0;
            entry[8 + extension++] = (u8)character;
        }
    }
    return base != 0;
}

static int save_file(fat16_file_t *file)
{
    if (!mounted || file == NULL || file->size > FAT16_DATA_SIZE - 1) {
        return 0;
    }
    if (file->size != 0 && file->first_cluster < 2) {
        file->first_cluster = allocate_cluster();
        if (file->first_cluster < 2) return 0;
    }

    if (file->first_cluster >= 2) {
        u8 sector[SECTOR_SIZE];
        for (u32 index = 0; index < SECTOR_SIZE; ++index) sector[index] = 0;
        for (u32 index = 0; index < file->size; ++index) sector[index] = (u8)file->data[index];
        u32 file_sector = data_start + (u32)(file->first_cluster - 2) * sectors_per_cluster;
        if (!write_volume_sector(file_sector, sector)) return 0;
    }

    u8 directory[SECTOR_SIZE];
    if (!read_volume_sector(file->directory_sector, directory)) return 0;
    u8 *entry = &directory[file->directory_offset];
    put_u16(&entry[26], file->first_cluster);
    put_u32(&entry[28], file->size);
    return write_volume_sector(file->directory_sector, directory);
}

int fat16_create(const char *name)
{
    if (!mounted || file_count >= FAT16_MAX_FILES || name == NULL) return -1;

    u8 short_name[11];
    if (!encode_name(name, short_name)) return -1;

    u32 free_sector = 0xFFFFFFFFu;
    u8 free_offset = 0;
    int directory_end = 0;
    for (u32 root_sector = 0; root_sector < root_sectors; ++root_sector) {
        u8 sector[SECTOR_SIZE];
        if (!read_volume_sector(root_start + root_sector, sector)) return -1;
        for (u32 offset = 0; offset < SECTOR_SIZE; offset += 32) {
            u8 *entry = &sector[offset];
            if (entry[0] == 0 || entry[0] == 0xE5) {
                if (free_sector == 0xFFFFFFFFu) {
                    free_sector = root_start + root_sector;
                    free_offset = (u8)offset;
                }
                if (entry[0] == 0) {
                    directory_end = 1;
                    break;
                }
                continue;
            }
            int same_name = 1;
            for (u32 name_index = 0; name_index < 11; ++name_index) {
                if (entry[name_index] != short_name[name_index]) {
                    same_name = 0;
                    break;
                }
            }
            if (same_name) return -1;
        }
        if (directory_end) break;
    }
    if (free_sector == 0xFFFFFFFFu) return -1;

    u8 sector[SECTOR_SIZE];
    if (!read_volume_sector(free_sector, sector)) return -1;
    for (u32 index = 0; index < 32; ++index) sector[free_offset + index] = 0;
    for (u32 index = 0; index < 11; ++index) sector[free_offset + index] = short_name[index];
    sector[free_offset + 11] = 0x20;
    if (!write_volume_sector(free_sector, sector)) return -1;

    u32 index = file_count++;
    fat16_file_t *file = &files[index];
    u32 name_size = 0;
    while (name[name_size] != '\0' && name_size + 1 < sizeof(file->name)) {
        char character = name[name_size];
        file->name[name_size] = character >= 'a' && character <= 'z'
            ? (char)(character - 'a' + 'A') : character;
        ++name_size;
    }
    file->name[name_size] = '\0';
    file->data[0] = '\0';
    file->size = 0;
    file->first_cluster = 0;
    file->directory_sector = free_sector;
    file->directory_offset = free_offset;
    file->used = 1;
    return (int)index;
}

int fat16_rename(u32 index, const char *name)
{
    fat16_file_t *file = fat16_file_at(index);
    if (!mounted || file == NULL || name == NULL) return 0;

    u8 short_name[11];
    if (!encode_name(name, short_name)) return 0;
    for (u32 other_index = 0; other_index < file_count; ++other_index) {
        if (other_index == index) continue;
        u8 other_name[11];
        if (!encode_name(files[other_index].name, other_name)) continue;
        int same_name = 1;
        for (u32 byte = 0; byte < 11; ++byte) {
            if (other_name[byte] != short_name[byte]) {
                same_name = 0;
                break;
            }
        }
        if (same_name) return 0;
    }

    u8 sector[SECTOR_SIZE];
    if (!read_volume_sector(file->directory_sector, sector)) return 0;
    u8 *entry = &sector[file->directory_offset];
    for (u32 byte = 0; byte < 11; ++byte) {
        entry[byte] = short_name[byte];
    }
    if (!write_volume_sector(file->directory_sector, sector)) return 0;

    u32 output = 0;
    for (u32 byte = 0; byte < 8 && short_name[byte] != ' '; ++byte) {
        file->name[output++] = (char)short_name[byte];
    }
    if (short_name[8] != ' ') {
        file->name[output++] = '.';
        for (u32 byte = 8; byte < 11 && short_name[byte] != ' '; ++byte) {
            file->name[output++] = (char)short_name[byte];
        }
    }
    file->name[output] = '\0';
    return 1;
}

int fat16_append(u32 index, char character)
{
    fat16_file_t *file = fat16_file_at(index);
    if (file == NULL || file->size + 1 >= FAT16_DATA_SIZE) return 0;
    file->data[file->size++] = character;
    file->data[file->size] = '\0';
    return save_file(file);
}

int fat16_backspace(u32 index)
{
    fat16_file_t *file = fat16_file_at(index);
    if (file == NULL || file->size == 0) return 0;
    file->data[--file->size] = '\0';
    return save_file(file);
}
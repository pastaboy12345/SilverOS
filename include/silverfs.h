#ifndef SILVEROS_SILVERFS_H
#define SILVEROS_SILVERFS_H

#include "types.h"

#define SILVERFS_MAGIC      0x53494C56  /* "SILV" */
#define SILVERFS_BLOCK_SIZE 4096
#define SILVERFS_MAX_NAME   255
#define SILVERFS_DIRECT_BLOCKS 12
#define SILVERFS_MAX_INODES 1024
#define SILVERFS_MAX_BLOCKS 4096
#define SILVERFS_MAX_FILES  256

/* Inode types */
#define SILVERFS_TYPE_FILE  1
#define SILVERFS_TYPE_DIR   2

/* Superblock */
typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t inode_table_block;
    uint32_t data_block_start;
    uint32_t root_inode;
    uint8_t  volume_name[32];
} silverfs_superblock_t;

/* Inode */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t block_count;
    uint32_t blocks[SILVERFS_DIRECT_BLOCKS];
    uint32_t indirect_block;
    uint32_t created_time;
    uint32_t modified_time;
    uint16_t permissions;
    uint16_t link_count;
} silverfs_inode_t;

/* Directory entry */
typedef struct {
    uint32_t inode;
    uint8_t  name_len;
    char     name[SILVERFS_MAX_NAME];
} silverfs_dirent_t;

/* File handle */
typedef struct {
    uint32_t inode_idx;
    uint32_t offset;
    bool     open;
} silverfs_file_t;

/* SilverFS operations */
int  silverfs_format(void);
int  silverfs_mount(void);
int  silverfs_create(const char *path, uint32_t type);
int  silverfs_open(const char *path, silverfs_file_t *file);
int  silverfs_read(silverfs_file_t *file, void *buf, size_t count);
int  silverfs_write(silverfs_file_t *file, const void *buf, size_t count);
void silverfs_close(silverfs_file_t *file);
int  silverfs_mkdir(const char *path);
int  silverfs_readdir(const char *path, silverfs_dirent_t *entries, int max_entries);
int  silverfs_delete(const char *path);
int  silverfs_stat(const char *path, silverfs_inode_t *inode);

#endif

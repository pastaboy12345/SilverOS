#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SILVERFS_MAGIC      0x53494C56  /* "SILV" */
#define SILVERFS_BLOCK_SIZE 4096
#define SILVERFS_MAX_NAME   255
#define SILVERFS_DIRECT_BLOCKS 12
#define SILVERFS_MAX_INODES 1024
#define SILVERFS_MAX_BLOCKS 4096

#define SILVERFS_TYPE_FILE  1
#define SILVERFS_TYPE_DIR   2

#define SVD_FILE_SIZE (16 * 1024 * 1024) /* 16 MB max for this demo layout */

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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <output.svd>\n", argv[0]);
        return 1;
    }

    const char *outfile = argv[1];
    FILE *f = fopen(outfile, "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    /* 1. Allocate full file size */
    uint8_t *disk = calloc(1, SVD_FILE_SIZE);
    if (!disk) {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return 1;
    }

    /* 2. Setup Superblock (Block 0) */
    silverfs_superblock_t *sb = (silverfs_superblock_t *)disk;
    sb->magic = SILVERFS_MAGIC;
    sb->block_size = SILVERFS_BLOCK_SIZE;
    sb->total_blocks = SILVERFS_MAX_BLOCKS;
    sb->total_inodes = SILVERFS_MAX_INODES;
    sb->free_blocks = SILVERFS_MAX_BLOCKS;
    sb->free_inodes = SILVERFS_MAX_INODES;
    strcpy((char *)sb->volume_name, "SilverOS");

    /* Layout in LBAs (512-byte sectors):
     * - Block 0: Superblock (offset 0, size 4096 bytes)
     * - Block 1: Inode Bitmap (offset 4096, size 4096. 1024 bits = 128 bytes used)
     * - Block 2: Block Bitmap (offset 8192, size 4096. 4096 bits = 512 bytes used)
     * - Block 3..N: Inode Table (offset 12288. 1024 inodes * 76 bytes = 77824 bytes = 19 blocks)
     * - Block 22+: Data blocks
     */
    
    int block_offset_inode_bmp = 1;
    int block_offset_block_bmp = 2;
    int block_offset_inode_tbl = 3;
    int blocks_for_inodes = (SILVERFS_MAX_INODES * sizeof(silverfs_inode_t) + SILVERFS_BLOCK_SIZE - 1) / SILVERFS_BLOCK_SIZE;
    int block_offset_data = block_offset_inode_tbl + blocks_for_inodes;
    
    sb->inode_table_block = block_offset_inode_tbl;
    sb->data_block_start = block_offset_data;

    uint8_t *inode_bmp = disk + (block_offset_inode_bmp * SILVERFS_BLOCK_SIZE);
    uint8_t *block_bmp = disk + (block_offset_block_bmp * SILVERFS_BLOCK_SIZE);
    silverfs_inode_t *inodes = (silverfs_inode_t *)(disk + (block_offset_inode_tbl * SILVERFS_BLOCK_SIZE));

    /* 3. Mark structural blocks as allocated in block bitmap */
    for (int i = 0; i < block_offset_data; i++) {
        block_bmp[i / 8] |= (1 << (i % 8));
        sb->free_blocks--;
    }

    /* 4. Allocate Root Inode (Inode 0) */
    sb->root_inode = 0;
    inode_bmp[0] |= 1;
    sb->free_inodes--;

    inodes[0].type = SILVERFS_TYPE_DIR;
    inodes[0].permissions = 0755;
    inodes[0].link_count = 2;
    inodes[0].size = 0;
    
    /* Allocate 1 data block for root dir */
    int root_data_blk = block_offset_data;
    block_bmp[root_data_blk / 8] |= (1 << (root_data_blk % 8));
    sb->free_blocks--;
    
    inodes[0].blocks[0] = root_data_blk;
    inodes[0].block_count = 1;

    /* 5. Write to file */
    fwrite(disk, 1, SVD_FILE_SIZE, f);
    fclose(f);
    free(disk);

    printf("Created SilverOS virtual disk: %s (16 MB)\n", outfile);
    return 0;
}

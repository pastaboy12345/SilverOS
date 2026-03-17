#include "../include/silverfs.h"
#include "../include/ata.h"
#include "../include/heap.h"
#include "../include/serial.h"
#include "../include/string.h"

/*
 * SilverFS — Persistent block device filesystem for SilverOS
 *
 * Layout (in LBAs / 512-byte sectors):
 * - Block 0: Superblock (offset 0, size 4096 bytes / 8 sectors)
 * - Block 1: Inode Bitmap (offset 4096, size 4096 / 8 sectors. 1024 bits = 128
 * bytes used)
 * - Block 2: Block Bitmap (offset 8192, size 4096 / 8 sectors. 4096 bits = 512
 * bytes used)
 * - Block 3..N: Inode Table (offset 12288. 1024 inodes * 76 bytes = 77824 bytes
 * = 19 blocks)
 * - Block 22+: Data blocks
 */

static silverfs_superblock_t superblock;
/* We don't cache all inodes/bitmaps in RAM anymore to ensure persistence,
 * but for simplicity of this demo, we'll cache bitmaps and write them back.
 * Ideally we'd read/modify/write sectors directly.
 */
static uint8_t inode_bitmap[SILVERFS_MAX_INODES / 8];
static uint8_t block_bitmap[SILVERFS_MAX_BLOCKS / 8];

#define SECTORS_PER_BLOCK (SILVERFS_BLOCK_SIZE / ATA_SECTOR_SIZE)
#define BLOCK_TO_LBA(block) ((block) * SECTORS_PER_BLOCK)

static uint32_t silverfs_lba_offset = 0;

/* --- Disk I/O Helpers --- */

static void read_block(uint32_t block, void *buf) {
  ata_read_sectors(BLOCK_TO_LBA(block) + silverfs_lba_offset, SECTORS_PER_BLOCK,
                   (uint8_t *)buf);
}

static void write_block(uint32_t block, const void *buf) {
  ata_write_sectors(BLOCK_TO_LBA(block) + silverfs_lba_offset,
                    SECTORS_PER_BLOCK, (const uint8_t *)buf);
}

static void save_bitmaps(void) {
  /* Block 1 is inode bitmap, Block 2 is block bitmap */
  uint8_t buf[SILVERFS_BLOCK_SIZE];

  memset(buf, 0, SILVERFS_BLOCK_SIZE);
  memcpy(buf, inode_bitmap, sizeof(inode_bitmap));
  write_block(1, buf);

  memset(buf, 0, SILVERFS_BLOCK_SIZE);
  memcpy(buf, block_bitmap, sizeof(block_bitmap));
  write_block(2, buf);

  /* Save superblock to Block 0 just in case free counts changed */
  memset(buf, 0, SILVERFS_BLOCK_SIZE);
  memcpy(buf, &superblock, sizeof(silverfs_superblock_t));
  write_block(0, buf);
}

static void read_inode(int idx, silverfs_inode_t *inode) {
  if (idx < 0 || idx >= SILVERFS_MAX_INODES)
    return;

  int inodes_per_block = SILVERFS_BLOCK_SIZE / sizeof(silverfs_inode_t);
  uint32_t block_idx = superblock.inode_table_block + (idx / inodes_per_block);
  int offset_in_block = idx % inodes_per_block;

  uint8_t buf[SILVERFS_BLOCK_SIZE];
  read_block(block_idx, buf);

  silverfs_inode_t *table = (silverfs_inode_t *)buf;
  *inode = table[offset_in_block];
}

static void write_inode(int idx, const silverfs_inode_t *inode) {
  if (idx < 0 || idx >= SILVERFS_MAX_INODES)
    return;

  int inodes_per_block = SILVERFS_BLOCK_SIZE / sizeof(silverfs_inode_t);
  uint32_t block_idx = superblock.inode_table_block + (idx / inodes_per_block);
  int offset_in_block = idx % inodes_per_block;

  /* Read-modify-write */
  uint8_t buf[SILVERFS_BLOCK_SIZE];
  read_block(block_idx, buf);

  silverfs_inode_t *table = (silverfs_inode_t *)buf;
  table[offset_in_block] = *inode;

  write_block(block_idx, buf);
}

/* --- Bitmap helpers --- */

static void bmp_set(uint8_t *bmp, int idx) { bmp[idx / 8] |= (1 << (idx % 8)); }
static void bmp_clear(uint8_t *bmp, int idx) {
  bmp[idx / 8] &= ~(1 << (idx % 8));
}
static bool bmp_test(uint8_t *bmp, int idx) {
  return bmp[idx / 8] & (1 << (idx % 8));
}

static int alloc_inode(void) {
  for (int i = 0; i < SILVERFS_MAX_INODES; i++) {
    if (!bmp_test(inode_bitmap, i)) {
      bmp_set(inode_bitmap, i);
      superblock.free_inodes--;
      save_bitmaps();

      silverfs_inode_t new_inode;
      memset(&new_inode, 0, sizeof(silverfs_inode_t));
      write_inode(i, &new_inode);
      return i;
    }
  }
  return -1;
}

static int alloc_block(void) {
  for (int i = 0; i < SILVERFS_MAX_BLOCKS; i++) {
    /* Prevent allocating structural blocks */
    if (i < (int)superblock.data_block_start)
      continue;

    if (!bmp_test(block_bitmap, i)) {
      bmp_set(block_bitmap, i);
      superblock.free_blocks--;
      save_bitmaps();

      /* Zero the block on disk */
      uint8_t buf[SILVERFS_BLOCK_SIZE];
      memset(buf, 0, SILVERFS_BLOCK_SIZE);
      write_block(i, buf);

      return i;
    }
  }
  return -1;
}

static void free_block(int idx) {
  if (idx >= (int)superblock.data_block_start && idx < SILVERFS_MAX_BLOCKS) {
    bmp_clear(block_bitmap, idx);
    superblock.free_blocks++;
    save_bitmaps();
  }
}

/* --- Path resolution --- */

static int find_in_dir(int dir_inode_idx, const char *name) {
  silverfs_inode_t dir;
  read_inode(dir_inode_idx, &dir);

  if (dir.type != SILVERFS_TYPE_DIR)
    return -1;

  for (int b = 0; b < SILVERFS_DIRECT_BLOCKS && dir.blocks[b]; b++) {
    uint8_t buf[SILVERFS_BLOCK_SIZE];
    read_block(dir.blocks[b], buf);
    silverfs_dirent_t *entries = (silverfs_dirent_t *)buf;

    int max = SILVERFS_BLOCK_SIZE / sizeof(silverfs_dirent_t);
    for (int i = 0; i < max; i++) {
      if (entries[i].inode && strcmp(entries[i].name, name) == 0) {
        return entries[i].inode;
      }
    }
  }
  return -1;
}

static int resolve_path(const char *path) {
  if (!path || path[0] != '/')
    return -1;
  if (path[0] == '/' && path[1] == 0)
    return superblock.root_inode;

  int current = superblock.root_inode;
  char component[SILVERFS_MAX_NAME + 1];

  path++; /* Skip leading / */

  while (*path) {
    int i = 0;
    while (*path && *path != '/')
      component[i++] = *path++;
    component[i] = 0;
    if (*path == '/')
      path++;
    if (i == 0)
      continue;

    current = find_in_dir(current, component);
    if (current < 0)
      return -1;
  }

  return current;
}

static int add_dirent(int dir_inode_idx, const char *name, int child_inode) {
  silverfs_inode_t dir;
  read_inode(dir_inode_idx, &dir);

  for (int b = 0; b < SILVERFS_DIRECT_BLOCKS; b++) {
    if (!dir.blocks[b]) {
      int blk = alloc_block();
      if (blk < 0)
        return -1;
      dir.blocks[b] = blk;
      dir.block_count++;
      write_inode(dir_inode_idx, &dir);
    }

    uint8_t buf[SILVERFS_BLOCK_SIZE];
    read_block(dir.blocks[b], buf);
    silverfs_dirent_t *entries = (silverfs_dirent_t *)buf;

    int max = SILVERFS_BLOCK_SIZE / sizeof(silverfs_dirent_t);
    for (int i = 0; i < max; i++) {
      if (entries[i].inode == 0) {
        entries[i].inode = child_inode;
        entries[i].name_len = strlen(name);
        strncpy(entries[i].name, name, SILVERFS_MAX_NAME);

        dir.size += sizeof(silverfs_dirent_t);
        write_inode(dir_inode_idx, &dir);
        write_block(dir.blocks[b], buf);
        return 0;
      }
    }
  }
  return -1;
}

/* --- Get parent dir and basename from path --- */
static int resolve_parent(const char *path, char *basename) {
  if (!path || path[0] != '/')
    return -1;

  /* Find last component */
  const char *last_slash = path;
  for (const char *p = path + 1; *p; p++) {
    if (*p == '/' && *(p + 1))
      last_slash = p;
  }

  /* Get parent path */
  char parent_path[512];
  if (last_slash == path) {
    /* Parent is root */
    strcpy(parent_path, "/");
    strcpy(basename, path + 1);
    /* Remove trailing slash from basename */
    char *s = strchr(basename, '/');
    if (s)
      *s = 0;
  } else {
    int len = last_slash - path;
    strncpy(parent_path, path, len);
    parent_path[len] = 0;
    strcpy(basename, last_slash + 1);
    char *s = strchr(basename, '/');
    if (s)
      *s = 0;
  }

  return resolve_path(parent_path);
}

/* --- Public API --- */

int silverfs_format(void) {
  /* Since mkfs_svd formats the disk on the host, we should rarely need this in
   * kernel. It's a stub just in case we need to reformat a corrupted disk.
   */
  serial_printf("[SilverFS] Format requested, but we rely on mkfs_svd host "
                "tool. Skipping.\n");
  return -1;
}

int silverfs_mount(void) {
  uint8_t buf[SILVERFS_BLOCK_SIZE];

  silverfs_lba_offset = 0;
  read_block(0, buf);
  memcpy(&superblock, buf, sizeof(silverfs_superblock_t));
  if (superblock.magic == SILVERFS_MAGIC)
    goto mount_ok;

  silverfs_lba_offset = 131072;
  read_block(0, buf);
  memcpy(&superblock, buf, sizeof(silverfs_superblock_t));
  if (superblock.magic == SILVERFS_MAGIC)
    goto mount_ok;

  silverfs_lba_offset = 0;
  serial_printf(
      "[SilverFS] ERROR: Invalid magic. Disk not formatted properly.\n");
  return -1;

mount_ok:
  /* Load bitmaps into memory */
  read_block(1, buf);
  memcpy(inode_bitmap, buf, sizeof(inode_bitmap));

  read_block(2, buf);
  memcpy(block_bitmap, buf, sizeof(block_bitmap));

  serial_printf("[SilverFS] Mounted SVD Volume '%s' (%d blocks total)\n",
                superblock.volume_name, superblock.total_blocks);
  return 0;
}

int silverfs_create(const char *path, uint32_t type) {
  char basename[SILVERFS_MAX_NAME + 1];
  int parent = resolve_parent(path, basename);
  if (parent < 0)
    return -1;

  /* Check if already exists */
  if (find_in_dir(parent, basename) >= 0)
    return -1;

  int ino = alloc_inode();
  if (ino < 0)
    return -1;

  silverfs_inode_t new_inode;
  memset(&new_inode, 0, sizeof(silverfs_inode_t));
  new_inode.type = type;
  new_inode.permissions = 0644;
  new_inode.link_count = 1;

  if (type == SILVERFS_TYPE_DIR) {
    int blk = alloc_block();
    if (blk < 0)
      return -1;
    new_inode.blocks[0] = blk;
    new_inode.block_count = 1;
    new_inode.permissions = 0755;
  }

  write_inode(ino, &new_inode);
  return add_dirent(parent, basename, ino);
}

int silverfs_open(const char *path, silverfs_file_t *file) {
  int ino = resolve_path(path);
  if (ino < 0)
    return -1;

  file->inode_idx = ino;
  file->offset = 0;
  file->open = true;
  return 0;
}

int silverfs_read(silverfs_file_t *file, void *buf, size_t count) {
  if (!file->open)
    return -1;

  silverfs_inode_t inode;
  read_inode(file->inode_idx, &inode);

  if (file->offset >= inode.size)
    return 0;
  if (file->offset + count > inode.size)
    count = inode.size - file->offset;

  size_t bytes_read = 0;
  uint8_t *dest = (uint8_t *)buf;

  while (bytes_read < count) {
    int block_idx = file->offset / SILVERFS_BLOCK_SIZE;
    int block_off = file->offset % SILVERFS_BLOCK_SIZE;

    if (block_idx >= SILVERFS_DIRECT_BLOCKS || !inode.blocks[block_idx])
      break;

    int to_read = SILVERFS_BLOCK_SIZE - block_off;
    if ((size_t)to_read > count - bytes_read)
      to_read = count - bytes_read;

    uint8_t bbuf[SILVERFS_BLOCK_SIZE];
    read_block(inode.blocks[block_idx], bbuf);
    memcpy(dest + bytes_read, bbuf + block_off, to_read);

    bytes_read += to_read;
    file->offset += to_read;
  }

  return bytes_read;
}

int silverfs_write(silverfs_file_t *file, const void *buf, size_t count) {
  if (!file->open)
    return -1;

  silverfs_inode_t inode;
  read_inode(file->inode_idx, &inode);

  size_t bytes_written = 0;
  const uint8_t *src = (const uint8_t *)buf;

  while (bytes_written < count) {
    int block_idx = file->offset / SILVERFS_BLOCK_SIZE;
    int block_off = file->offset % SILVERFS_BLOCK_SIZE;

    if (block_idx >= SILVERFS_DIRECT_BLOCKS)
      break;

    if (!inode.blocks[block_idx]) {
      int blk = alloc_block();
      if (blk < 0)
        break;
      inode.blocks[block_idx] = blk;
      inode.block_count++;
      write_inode(file->inode_idx, &inode);
    }

    int to_write = SILVERFS_BLOCK_SIZE - block_off;
    if ((size_t)to_write > count - bytes_written)
      to_write = count - bytes_written;

    uint8_t bbuf[SILVERFS_BLOCK_SIZE];
    read_block(inode.blocks[block_idx],
               bbuf); /* Read to preserve other data in block */
    memcpy(bbuf + block_off, src + bytes_written, to_write);
    write_block(inode.blocks[block_idx], bbuf);

    bytes_written += to_write;
    file->offset += to_write;

    if (file->offset > inode.size) {
      inode.size = file->offset;
      write_inode(file->inode_idx, &inode);
    }
  }

  return bytes_written;
}

void silverfs_close(silverfs_file_t *file) { file->open = false; }

int silverfs_mkdir(const char *path) {
  return silverfs_create(path, SILVERFS_TYPE_DIR);
}

int silverfs_readdir(const char *path, silverfs_dirent_t *entries,
                     int max_entries) {
  int ino = resolve_path(path);
  if (ino < 0)
    return -1;

  silverfs_inode_t dir;
  read_inode(ino, &dir);

  if (dir.type != SILVERFS_TYPE_DIR)
    return -1;

  int count = 0;
  for (int b = 0; b < SILVERFS_DIRECT_BLOCKS && dir.blocks[b]; b++) {
    uint8_t bbuf[SILVERFS_BLOCK_SIZE];
    read_block(dir.blocks[b], bbuf);

    silverfs_dirent_t *dentries = (silverfs_dirent_t *)bbuf;
    int max = SILVERFS_BLOCK_SIZE / sizeof(silverfs_dirent_t);
    for (int i = 0; i < max && count < max_entries; i++) {
      if (dentries[i].inode) {
        entries[count++] = dentries[i];
      }
    }
  }
  return count;
}

int silverfs_delete(const char *path) {
  char basename[SILVERFS_MAX_NAME + 1];
  int parent = resolve_parent(path, basename);
  if (parent < 0)
    return -1;

  int ino = find_in_dir(parent, basename);
  if (ino < 0)
    return -1;

  silverfs_inode_t inode;
  read_inode(ino, &inode);

  /* Free blocks */
  for (int i = 0; i < SILVERFS_DIRECT_BLOCKS; i++) {
    if (inode.blocks[i])
      free_block(inode.blocks[i]);
  }

  /* Free inode */
  bmp_clear(inode_bitmap, ino);
  superblock.free_inodes++;
  save_bitmaps();

  memset(&inode, 0, sizeof(silverfs_inode_t));
  write_inode(ino, &inode);

  /* Remove from parent directory */
  silverfs_inode_t dir;
  read_inode(parent, &dir);
  for (int b = 0; b < SILVERFS_DIRECT_BLOCKS && dir.blocks[b]; b++) {
    uint8_t bbuf[SILVERFS_BLOCK_SIZE];
    read_block(dir.blocks[b], bbuf);

    silverfs_dirent_t *dentries = (silverfs_dirent_t *)bbuf;
    int max = SILVERFS_BLOCK_SIZE / sizeof(silverfs_dirent_t);
    for (int i = 0; i < max; i++) {
      if (dentries[i].inode == (uint32_t)ino &&
          strcmp(dentries[i].name, basename) == 0) {
        memset(&dentries[i], 0, sizeof(silverfs_dirent_t));
        write_block(dir.blocks[b], bbuf);
        return 0;
      }
    }
  }

  return 0;
}

int silverfs_stat(const char *path, silverfs_inode_t *inode) {
  int ino = resolve_path(path);
  if (ino < 0)
    return -1;
  read_inode(ino, inode);
  return 0;
}

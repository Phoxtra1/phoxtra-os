#include "../kernel/storage/ext2.h"
#include "../kernel/storage/blockdev.h"
#include "../kernel/vfs/vfs.h"
#include "../kernel/memory/heap.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BLOCK_SIZE 1024u
#define TEST_BLOCKS 64u
#define TEST_IMAGE_BYTES (TEST_BLOCK_SIZE * TEST_BLOCKS)
#define TEST_SECTORS (TEST_IMAGE_BYTES / 512u)

typedef struct memory_disk {
    uint8_t* bytes;
    uint64_t size;
    unsigned read_calls;
    unsigned write_calls;
} memory_disk_t;

static uint8_t valid_image[TEST_IMAGE_BYTES];
static uint8_t corrupt_image[TEST_IMAGE_BYTES];
static uint8_t unsupported_image[TEST_IMAGE_BYTES];
static memory_disk_t valid_disk = {valid_image, sizeof(valid_image), 0, 0};
static memory_disk_t corrupt_disk = {corrupt_image, sizeof(corrupt_image), 0, 0};
static memory_disk_t unsupported_disk = {unsupported_image, sizeof(unsupported_image), 0, 0};

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }

static void put16(uint8_t* at, uint16_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
}
static void put32(uint8_t* at, uint32_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
}
static int disk_read(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->priv_data || !buffer || !count ||
        lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba) return -1;
    memory_disk_t* disk = (memory_disk_t*)dev->priv_data;
    uint64_t offset = lba * dev->sector_size;
    uint64_t length = (uint64_t)count * dev->sector_size;
    if (offset > disk->size || length > disk->size - offset) return -1;
    ++disk->read_calls;
    memcpy(buffer, disk->bytes + offset, (size_t)length);
    return 0;
}
static int disk_write(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)lba; (void)count; (void)buffer;
    if (dev && dev->priv_data) ++((memory_disk_t*)dev->priv_data)->write_calls;
    return -1;
}
static void setup_device(block_device_t* dev, const char* name, memory_disk_t* private_data) {
    memset(dev, 0, sizeof(*dev));
    size_t i = 0;
    while (name[i] && i < sizeof(dev->name) - 1u) {
        dev->name[i] = name[i];
        ++i;
    }
    dev->name[i] = '\0';
    dev->total_sectors = TEST_SECTORS;
    dev->sector_size = 512u;
    dev->flags = BLOCKDEV_FLAG_READONLY | BLOCKDEV_FLAG_PARTITION;
    dev->read_sectors = disk_read;
    dev->write_sectors = disk_write;
    dev->priv_data = private_data;
}
static void set_dir_entry(uint8_t* directory, uint32_t offset, uint32_t ino,
                          uint16_t record_length, const char* name) {
    size_t name_length = strlen(name);
    put32(directory + offset, ino);
    put16(directory + offset + 4, record_length);
    /* Old ext2 directory entry layout: name_len is a 16-bit value. */
    put16(directory + offset + 6, (uint16_t)name_length);
    memcpy(directory + offset + 8, name, name_length);
}
static void set_inode(uint8_t* image, uint32_t inode, uint16_t mode,
                      uint32_t size, uint32_t first_block, uint16_t links) {
    uint8_t* data = image + 5u * TEST_BLOCK_SIZE + (uint64_t)(inode - 1u) * 128u;
    memset(data, 0, 128);
    put16(data, mode);
    put32(data + 4, size);
    put16(data + 26, links);
    put32(data + 28, 2u); /* i_blocks in 512-byte sectors */
    put32(data + 40, first_block);
}
static void make_valid_ext2(uint8_t* image) {
    memset(image, 0, TEST_IMAGE_BYTES);
    uint8_t* sb = image + 1024u;
    put32(sb + 0, 16u);  /* inode count */
    put32(sb + 4, TEST_BLOCKS);
    put32(sb + 20, 1u);  /* first data block for 1 KiB blocks */
    put32(sb + 24, 0u);  /* log_block_size */
    put32(sb + 32, 63u); /* blocks per group */
    put32(sb + 40, 16u); /* inodes per group */
    put16(sb + 56, EXT2_SUPER_MAGIC);
    put32(sb + 76, 0u);  /* revision 0: 128-byte inodes */

    /* Group descriptor table is block 2; inode table starts at block 5. */
    put32(image + 2u * TEST_BLOCK_SIZE + 8u, 5u);

    /* Inodes 2..5: root directory, root file, subdirectory, nested file. */
    set_inode(image, 2u, 0x41EDu, TEST_BLOCK_SIZE, 7u, 2u);
    set_inode(image, 3u, 0x81A4u, 22u, 8u, 1u);
    set_inode(image, 4u, 0x41EDu, TEST_BLOCK_SIZE, 9u, 2u);
    set_inode(image, 5u, 0x81A4u, 16u, 10u, 1u);

    uint8_t* root = image + 7u * TEST_BLOCK_SIZE;
    set_dir_entry(root, 0u, 2u, 12u, ".");
    set_dir_entry(root, 12u, 2u, 12u, "..");
    set_dir_entry(root, 24u, 3u, 20u, "PHOXTRACI");
    set_dir_entry(root, 44u, 4u, 980u, "SUBDIR");

    uint8_t* subdir = image + 9u * TEST_BLOCK_SIZE;
    set_dir_entry(subdir, 0u, 4u, 12u, ".");
    set_dir_entry(subdir, 12u, 2u, 12u, "..");
    set_dir_entry(subdir, 24u, 5u, 1000u, "NESTED");

    memcpy(image + 8u * TEST_BLOCK_SIZE, "Phoxtra ext2 read OK\n", 22u);
    memcpy(image + 10u * TEST_BLOCK_SIZE, "Nested file OK\n", 16u);
}
#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

int main(void) {
    block_device_t valid_dev, corrupt_dev, unsupported_dev;
    uint8_t output[128];

    make_valid_ext2(valid_image);
    memcpy(corrupt_image, valid_image, sizeof(valid_image));
    memcpy(unsupported_image, valid_image, sizeof(valid_image));
    put16(corrupt_image + 7u * TEST_BLOCK_SIZE + 24u + 4u, 0u);
    put32(unsupported_image + 1024u + 96u, 0x80u);

    setup_device(&valid_dev, "disk0p1", &valid_disk);
    setup_device(&corrupt_dev, "bad0p1", &corrupt_disk);
    setup_device(&unsupported_dev, "bad1p1", &unsupported_disk);

    vfs_init();
    vfs_node_t* mount = ext2_mount(&valid_dev, "/disk0p1");
    CHECK("supported on-disk ext2 partition mounts", mount != NULL);
    vfs_node_t* file = vfs_lookup("/disk0p1/PHOXTRACI");
    CHECK("root directory entries resolve to the correct file", file != NULL && file->flags == VFS_FILE);
    memset(output, 0, sizeof(output));
    uint64_t n = vfs_read(file, 0, sizeof(output) - 1u, output);
    CHECK("on-disk file data is read through VFS", n == 22u && memcmp(output, "Phoxtra ext2 read OK\n", 22u) == 0);
    memset(output, 0, sizeof(output));
    n = vfs_read(file, 7u, 5u, output);
    CHECK("file reads support offsets and bounded lengths", n == 5u && memcmp(output, "ext2 ", 5u) == 0);
    vfs_node_t* nested = vfs_lookup("/disk0p1/SUBDIR/NESTED");
    CHECK("nested directories are parsed safely", nested != NULL && nested->flags == VFS_FILE);
    memset(output, 0, sizeof(output));
    n = vfs_read(nested, 0u, sizeof(output) - 1u, output);
    CHECK("nested file content is read correctly", n == 16u && memcmp(output, "Nested file OK\n", 16u) == 0);
    CHECK("VFS writes fail closed for ext2 files", vfs_write(file, 0u, 1u, (const uint8_t*)"x") == 0);
    CHECK("ext2 mount never calls the disk write callback",
          valid_disk.write_calls == 0 && corrupt_disk.write_calls == 0 && unsupported_disk.write_calls == 0);

    CHECK("corrupt directory record lengths reject the mount",
          ext2_mount(&corrupt_dev, "/bad0p1") == NULL && vfs_lookup("/bad0p1") == NULL);
    CHECK("unsupported incompatibility features reject the mount",
          ext2_mount(&unsupported_dev, "/bad1p1") == NULL && vfs_lookup("/bad1p1") == NULL);
    CHECK("failed mount attempts do not issue disk writes",
          corrupt_disk.write_calls == 0 && unsupported_disk.write_calls == 0);

    printf("Read-only ext2 regression tests passed.\n");
    return 0;
}

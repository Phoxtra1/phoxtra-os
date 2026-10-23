#include "ext2.h"
#include "../memory/heap.h"
#include "../log/logger.h"

/* Read-only ext2 subset for 512-byte-sector block devices.
   Journaling, indexed directories, unsupported feature bits and all writes are rejected. */
#define EXT2_MAX_MOUNTS 8u
#define EXT2_MAX_NODES 2048u
#define EXT2_MAX_DEPTH 24u
#define EXT2_SUPERBLOCK_OFFSET 1024u
#define EXT2_SUPERBLOCK_SIZE 1024u
#define EXT2_GROUP_DESC_SIZE 32u
#define EXT2_INODE_SIZE_MIN 128u
#define EXT2_ROOT_INODE 2u
#define EXT2_BLOCK_SIZE_MIN 1024u
#define EXT2_BLOCK_SIZE_MAX 4096u
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL 0x0004u
#define EXT2_FEATURE_COMPAT_DIR_INDEX   0x0020u
#define EXT2_FEATURE_INCOMPAT_FILETYPE  0x0002u
#define EXT2_FEATURE_RO_COMPAT_SUPPORTED 0x0003u
#define EXT2_INDEX_FL 0x00001000u
#define EXT2_S_IFMT  0xF000u
#define EXT2_S_IFREG 0x8000u
#define EXT2_S_IFDIR 0x4000u

typedef struct ext2_fs_context {
    block_device_t* device;
    uint32_t block_size;
    uint32_t blocks_count;
    uint32_t inodes_count;
    uint32_t first_data_block;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t groups_count;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint64_t filesystem_bytes;
    uint64_t group_desc_offset;
    uint32_t node_count;
    uint8_t active;
} ext2_fs_context_t;

typedef struct ext2_node_data {
    ext2_fs_context_t* fs;
    uint32_t inode_number;
    ext2_inode_t inode;
    uint64_t size;
} ext2_node_data_t;

typedef struct {
    char name[32];
    const char* content;
    uint64_t size;
} ext2_demo_file_t;

static ext2_fs_context_t ext2_contexts[EXT2_MAX_MOUNTS];

static const char boot_cfg_text[] =
    "TIMEOUT 5\n"
    "DEFAULT phoxtra\n\n"
    "LABEL phoxtra\n"
    "    KERNEL /boot/phoxtra\n"
    "    APPEND root=/dev/ramdisk0p1 ro loglevel=info\n";

static const char sys_config_text[] =
    "[PHOXTRA_SYSTEM_CONFIG]\n"
    "kernel.version = 0.7.0\n"
    "storage.backend = volatile_ram_demo\n"
    "security.tpm = status_reported_separately\n"
    "fs.root = in_memory_demo_adapter\n";

static void kstrncpy(char* dst, const char* src, int maxlen) {
    if (!dst || maxlen <= 0) return;
    int i = 0;
    if (src) for (; i < maxlen - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int strings_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t device_bytes(block_device_t* dev) {
    if (!dev || !dev->sector_size ||
        dev->total_sectors > UINT64_MAX / dev->sector_size) return 0;
    return dev->total_sectors * dev->sector_size;
}

static int ext2_read_bytes(block_device_t* dev, uint64_t offset,
                           uint64_t length, uint8_t* output) {
    if (!dev || !dev->read_sectors || !output || dev->sector_size != 512u) return -1;
    uint64_t bytes_available = device_bytes(dev);
    if (!bytes_available || offset > bytes_available || length > bytes_available - offset) return -1;

    while (length) {
        uint64_t lba = offset / 512u;
        uint32_t sector_offset = (uint32_t)(offset % 512u);
        if (lba >= dev->total_sectors) return -1;

        if (sector_offset == 0 && length >= 512u) {
            uint64_t sectors64 = length / 512u;
            uint32_t sectors = sectors64 > UINT32_MAX ? UINT32_MAX : (uint32_t)sectors64;
            if ((uint64_t)sectors > dev->total_sectors - lba) return -1;
            if (dev->read_sectors(dev, lba, sectors, output) != 0) return -1;
            uint64_t chunk = (uint64_t)sectors * 512u;
            offset += chunk;
            output += chunk;
            length -= chunk;
            continue;
        }

        uint8_t sector[512];
        if (dev->read_sectors(dev, lba, 1, sector) != 0) return -1;
        uint32_t available = 512u - sector_offset;
        uint64_t chunk = length < available ? length : available;
        for (uint64_t i = 0; i < chunk; ++i) output[i] = sector[sector_offset + i];
        offset += chunk;
        output += chunk;
        length -= chunk;
    }
    return 0;
}

static int ext2_read_block(ext2_fs_context_t* fs, uint32_t block,
                           uint8_t* output) {
    if (!fs || !output || block == 0 || block >= fs->blocks_count) return -1;
    uint64_t offset = (uint64_t)block * fs->block_size;
    if (offset > fs->filesystem_bytes ||
        fs->block_size > fs->filesystem_bytes - offset) return -1;
    return ext2_read_bytes(fs->device, offset, fs->block_size, output);
}

static int ext2_read_indirect_entry(ext2_fs_context_t* fs, uint32_t block,
                                    uint64_t index, uint32_t* value) {
    if (!fs || !value || block == 0 || block >= fs->blocks_count ||
        index >= fs->block_size / sizeof(uint32_t)) return -1;
    uint8_t raw[4];
    uint64_t offset = (uint64_t)block * fs->block_size + index * sizeof(uint32_t);
    if (ext2_read_bytes(fs->device, offset, sizeof(raw), raw) != 0) return -1;
    *value = le32(raw);
    if (*value >= fs->blocks_count) return -1;
    return 0;
}

/* Maps a logical file block through ext2's direct, single-, double- and triple-indirect pointers. */
static int ext2_get_data_block(ext2_fs_context_t* fs, const ext2_inode_t* inode,
                               uint64_t logical, uint32_t* block_out) {
    if (!fs || !inode || !block_out) return -1;
    *block_out = 0;
    if (logical < 12u) {
        *block_out = inode->i_block[logical];
        return *block_out < fs->blocks_count ? 0 : -1;
    }

    const uint64_t pointers_per_block = fs->block_size / sizeof(uint32_t);
    logical -= 12u;
    if (logical < pointers_per_block) {
        if (!inode->i_block[12]) return 0;
        return ext2_read_indirect_entry(fs, inode->i_block[12], logical, block_out);
    }

    logical -= pointers_per_block;
    const uint64_t double_capacity = pointers_per_block * pointers_per_block;
    if (logical < double_capacity) {
        if (!inode->i_block[13]) return 0;
        uint32_t indirect = 0;
        if (ext2_read_indirect_entry(fs, inode->i_block[13],
                                     logical / pointers_per_block, &indirect) != 0) return -1;
        if (!indirect) return 0;
        return ext2_read_indirect_entry(fs, indirect,
                                        logical % pointers_per_block, block_out);
    }

    logical -= double_capacity;
    const uint64_t triple_capacity = double_capacity * pointers_per_block;
    if (logical < triple_capacity) {
        if (!inode->i_block[14]) return 0;
        uint32_t first = 0, second = 0;
        if (ext2_read_indirect_entry(fs, inode->i_block[14],
                                     logical / double_capacity, &first) != 0) return -1;
        if (!first) return 0;
        uint64_t remainder = logical % double_capacity;
        if (ext2_read_indirect_entry(fs, first,
                                     remainder / pointers_per_block, &second) != 0) return -1;
        if (!second) return 0;
        return ext2_read_indirect_entry(fs, second,
                                        remainder % pointers_per_block, block_out);
    }
    return -1;
}

static int ext2_read_inode(ext2_fs_context_t* fs, uint32_t inode_number,
                           ext2_inode_t* inode_out) {
    if (!fs || !inode_out || inode_number == 0 || inode_number > fs->inodes_count) return -1;
    uint32_t group = (inode_number - 1u) / fs->inodes_per_group;
    uint32_t index = (inode_number - 1u) % fs->inodes_per_group;
    if (group >= fs->groups_count) return -1;

    uint8_t descriptor[EXT2_GROUP_DESC_SIZE];
    uint64_t desc_offset = fs->group_desc_offset + (uint64_t)group * EXT2_GROUP_DESC_SIZE;
    if (desc_offset > fs->filesystem_bytes ||
        EXT2_GROUP_DESC_SIZE > fs->filesystem_bytes - desc_offset ||
        ext2_read_bytes(fs->device, desc_offset, sizeof(descriptor), descriptor) != 0) return -1;

    uint32_t inode_table_block = le32(descriptor + 8);
    if (!inode_table_block || inode_table_block >= fs->blocks_count) return -1;
    uint64_t inode_offset = (uint64_t)inode_table_block * fs->block_size +
                            (uint64_t)index * fs->inode_size;
    if (inode_offset > fs->filesystem_bytes ||
        sizeof(*inode_out) > fs->filesystem_bytes - inode_offset ||
        ext2_read_bytes(fs->device, inode_offset, sizeof(*inode_out),
                        (uint8_t*)inode_out) != 0) return -1;
    return 0;
}

static uint64_t ext2_inode_size(const ext2_fs_context_t* fs, const ext2_inode_t* inode) {
    uint64_t size = inode->i_size;
    if ((inode->i_mode & EXT2_S_IFMT) == EXT2_S_IFREG &&
        (fs->feature_ro_compat & 0x0002u)) {
        size |= (uint64_t)inode->i_dir_acl << 32;
    }
    return size;
}

static uint64_t ext2_file_read(vfs_node_t* node, uint64_t offset,
                               uint64_t size, uint8_t* output) {
    if (!node || !node->ptr || !output || size == 0) return 0;
    ext2_node_data_t* data = (ext2_node_data_t*)node->ptr;
    ext2_fs_context_t* fs = data->fs;
    if (!fs || !fs->active || offset >= data->size) return 0;

    uint64_t bytes_to_read = size;
    if (bytes_to_read > data->size - offset) bytes_to_read = data->size - offset;
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) return 0;

    uint64_t done = 0;
    while (done < bytes_to_read) {
        uint64_t file_offset = offset + done;
        uint64_t logical_block = file_offset / fs->block_size;
        uint32_t in_block = (uint32_t)(file_offset % fs->block_size);
        uint64_t chunk = fs->block_size - in_block;
        if (chunk > bytes_to_read - done) chunk = bytes_to_read - done;

        uint32_t physical_block = 0;
        if (ext2_get_data_block(fs, &data->inode, logical_block, &physical_block) != 0) break;
        if (!physical_block) {
            for (uint64_t i = 0; i < chunk; ++i) output[done + i] = 0;
        } else {
            if (ext2_read_block(fs, physical_block, block_buffer) != 0) break;
            for (uint64_t i = 0; i < chunk; ++i) output[done + i] = block_buffer[in_block + i];
        }
        done += chunk;
    }

    kfree(block_buffer);
    return done;
}

static void ext2_free_tree(vfs_node_t* node) {
    if (!node) return;
    vfs_node_t* child = node->first_child;
    while (child) {
        vfs_node_t* next = child->next_sibling;
        ext2_free_tree(child);
        child = next;
    }
    if (node->ptr) kfree(node->ptr);
    kfree(node);
}

static int ext2_name_is_dot(const char* name) {
    return strings_equal(name, ".") || strings_equal(name, "..");
}

static int ext2_directory_contains(vfs_node_t* directory, const char* name) {
    for (vfs_node_t* child = directory ? directory->first_child : (void*)0;
         child; child = child->next_sibling) {
        if (strings_equal(child->name, name)) return 1;
    }
    return 0;
}

static int ext2_ancestor_has_inode(const uint32_t* ancestry, uint32_t depth,
                                   uint32_t inode_number) {
    for (uint32_t i = 0; i <= depth; ++i) {
        if (ancestry[i] == inode_number) return 1;
    }
    return 0;
}

static int ext2_build_directory(ext2_fs_context_t* fs, vfs_node_t* directory,
                                ext2_node_data_t* directory_data, uint32_t depth,
                                uint32_t ancestry[EXT2_MAX_DEPTH]) {
    if (!fs || !directory || !directory_data ||
        (directory_data->inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return -1;
    if (depth >= EXT2_MAX_DEPTH) return 0;

    /* Indexed directories need the ext2 htree walker; don't misparse index blocks as names. */
    if (directory_data->inode.i_flags & EXT2_INDEX_FL) {
        if (depth == 0) return -1;
        klog(LOG_WARNING, "Ext2: indexed subdirectory left unread because htree traversal is not enabled");
        return 0;
    }

    ancestry[depth] = directory_data->inode_number;
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    uint64_t directory_size = directory_data->size;
    uint64_t logical_blocks = directory_size / fs->block_size +
                              ((directory_size % fs->block_size) ? 1u : 0u);
    int result = 0;

    for (uint64_t logical = 0; logical < logical_blocks && result == 0; ++logical) {
        uint32_t physical = 0;
        if (ext2_get_data_block(fs, &directory_data->inode, logical, &physical) != 0 ||
            physical == 0 || ext2_read_block(fs, physical, block_buffer) != 0) {
            result = -1;
            break;
        }

        uint64_t file_block_offset = logical * fs->block_size;
        uint64_t valid_bytes = directory_size - file_block_offset;
        if (valid_bytes > fs->block_size) valid_bytes = fs->block_size;
        uint64_t pos = 0;

        while (pos < valid_bytes) {
            if (valid_bytes - pos < 8u) { result = -1; break; }
            const uint8_t* entry = block_buffer + pos;
            uint32_t child_inode = le32(entry);
            uint16_t record_length = le16(entry + 4);
            uint16_t name_length = (fs->feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE)
                ? entry[6] : le16(entry + 6);

            if (record_length < 8u || (record_length & 3u) != 0 ||
                record_length > valid_bytes - pos) {
                result = -1;
                break;
            }
            if (child_inode != 0) {
                if (child_inode > fs->inodes_count ||
                    name_length == 0 || name_length > record_length - 8u) {
                    result = -1;
                    break;
                }
                if (name_length < sizeof(((vfs_node_t*)0)->name)) {
                    char name[64];
                    int valid_name = 1;
                    for (uint16_t i = 0; i < name_length; ++i) {
                        name[i] = (char)entry[8u + i];
                        if (name[i] == '/' || name[i] == '\0') valid_name = 0;
                    }
                    name[name_length] = '\0';
                    if (valid_name && !ext2_name_is_dot(name) &&
                        !ext2_directory_contains(directory, name)) {
                        ext2_inode_t child_inode_data;
                        if (ext2_read_inode(fs, child_inode, &child_inode_data) != 0) {
                            result = -1;
                            break;
                        }
                        uint16_t mode = child_inode_data.i_mode & EXT2_S_IFMT;
                        uint32_t child_flags = (mode == EXT2_S_IFDIR) ? VFS_DIRECTORY :
                                               (mode == EXT2_S_IFREG) ? VFS_FILE : 0;
                        if (child_flags) {
                            if (fs->node_count >= EXT2_MAX_NODES) { result = -1; break; }
                            if (child_flags == VFS_DIRECTORY &&
                                ext2_ancestor_has_inode(ancestry, depth, child_inode)) {
                                klog(LOG_WARNING, "Ext2: cyclic directory entry skipped");
                                pos += record_length;
                                continue;
                            }

                            ext2_node_data_t* child_data =
                                (ext2_node_data_t*)kmalloc(sizeof(*child_data));
                            if (!child_data) { result = -1; break; }
                            child_data->fs = fs;
                            child_data->inode_number = child_inode;
                            child_data->inode = child_inode_data;
                            child_data->size = ext2_inode_size(fs, &child_inode_data);

                            vfs_node_t* child = vfs_create_child(directory, name, child_flags);
                            if (!child) {
                                kfree(child_data);
                                result = -1;
                                break;
                            }
                            child->ptr = child_data;
                            child->size = child_data->size;
                            child->permissions = child_inode_data.i_mode & 0777u;
                            if (child_flags == VFS_FILE) {
                                child->read = ext2_file_read;
                            } else if (depth + 1u < EXT2_MAX_DEPTH) {
                                if (ext2_build_directory(fs, child, child_data, depth + 1u,
                                                        ancestry) != 0) {
                                    result = -1;
                                    break;
                                }
                            } else {
                                klog(LOG_WARNING, "Ext2: maximum directory depth reached; deeper contents are not enumerated");
                            }
                            ++fs->node_count;
                        }
                    }
                }
            }
            pos += record_length;
        }
    }

    kfree(block_buffer);
    return result;
}

static ext2_fs_context_t* ext2_allocate_context(void) {
    for (uint32_t i = 0; i < EXT2_MAX_MOUNTS; ++i) {
        if (!ext2_contexts[i].active && !ext2_contexts[i].device) {
            for (size_t b = 0; b < sizeof(ext2_contexts[i]); ++b)
                ((uint8_t*)&ext2_contexts[i])[b] = 0;
            ext2_contexts[i].device = (block_device_t*)1; /* Reserved while mounting. */
            return &ext2_contexts[i];
        }
    }
    return (void*)0;
}

static void ext2_release_context(ext2_fs_context_t* fs) {
    if (!fs) return;
    for (size_t b = 0; b < sizeof(*fs); ++b) ((uint8_t*)fs)[b] = 0;
}

static int ext2_mount_path_valid(const char* path) {
    if (!path || path[0] != '/' || !path[1]) return 0;
    if (path[1] == '.' || path[1] == '/') return 0;
    for (uint32_t i = 1; path[i]; ++i) if (path[i] == '/') return 0;
    return 1;
}

static vfs_node_t* ext2_mount_readonly(block_device_t* dev, const char* mount_point) {
    if (!dev || !dev->read_sectors || dev->sector_size != 512u ||
        !(dev->flags & BLOCKDEV_FLAG_READONLY) ||
        !(dev->flags & BLOCKDEV_FLAG_PARTITION) ||
        (dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
        !ext2_mount_path_valid(mount_point) || vfs_lookup(mount_point)) return (void*)0;

    uint8_t super_raw[EXT2_SUPERBLOCK_SIZE];
    if (ext2_read_bytes(dev, EXT2_SUPERBLOCK_OFFSET, sizeof(super_raw), super_raw) != 0)
        return (void*)0;
    if (le16(super_raw + 56) != EXT2_SUPER_MAGIC) return (void*)0;

    uint32_t inodes_count = le32(super_raw + 0);
    uint32_t blocks_count = le32(super_raw + 4);
    uint32_t first_data_block = le32(super_raw + 20);
    uint32_t log_block_size = le32(super_raw + 24);
    uint32_t blocks_per_group = le32(super_raw + 32);
    uint32_t inodes_per_group = le32(super_raw + 40);
    uint32_t revision = le32(super_raw + 76);
    uint32_t inode_size = revision == 0 ? EXT2_INODE_SIZE_MIN : le16(super_raw + 88);
    uint32_t feature_compat = le32(super_raw + 92);
    uint32_t feature_incompat = le32(super_raw + 96);
    uint32_t feature_ro_compat = le32(super_raw + 100);
    uint64_t bytes_available = device_bytes(dev);

    if (!inodes_count || blocks_count < 2u || log_block_size > 2u ||
        !blocks_per_group || !inodes_per_group ||
        (inode_size < EXT2_INODE_SIZE_MIN) || inode_size > 256u ||
        (inode_size & 3u) != 0 || revision > 1u ||
        (feature_compat & (EXT2_FEATURE_COMPAT_HAS_JOURNAL | EXT2_FEATURE_COMPAT_DIR_INDEX)) ||
        (feature_incompat & ~EXT2_FEATURE_INCOMPAT_FILETYPE) ||
        (feature_ro_compat & ~EXT2_FEATURE_RO_COMPAT_SUPPORTED)) {
        klog(LOG_WARNING, "Ext2: unsupported or malformed superblock/features; partition left read-only and unmounted");
        return (void*)0;
    }

    uint32_t block_size = EXT2_BLOCK_SIZE_MIN << log_block_size;
    if ((block_size == EXT2_BLOCK_SIZE_MIN && first_data_block != 1u) ||
        (block_size > EXT2_BLOCK_SIZE_MIN && first_data_block != 0u)) {
        klog(LOG_WARNING, "Ext2: superblock data-block geometry is invalid");
        return (void*)0;
    }

    uint64_t filesystem_bytes = (uint64_t)blocks_count * block_size;
    if (filesystem_bytes > bytes_available || filesystem_bytes / block_size != blocks_count) {
        klog(LOG_WARNING, "Ext2: filesystem size exceeds its read-only partition");
        return (void*)0;
    }
    uint64_t block_groups = ((uint64_t)blocks_count - first_data_block +
                             blocks_per_group - 1u) / blocks_per_group;
    uint64_t inode_groups = ((uint64_t)inodes_count + inodes_per_group - 1u) / inodes_per_group;
    if (!block_groups || block_groups != inode_groups || block_groups > UINT32_MAX) {
        klog(LOG_WARNING, "Ext2: block/inode group geometry is inconsistent");
        return (void*)0;
    }

    ext2_fs_context_t* fs = ext2_allocate_context();
    if (!fs) {
        klog(LOG_WARNING, "Ext2: mount table is full");
        return (void*)0;
    }
    fs->device = dev;
    fs->block_size = block_size;
    fs->blocks_count = blocks_count;
    fs->inodes_count = inodes_count;
    fs->first_data_block = first_data_block;
    fs->blocks_per_group = blocks_per_group;
    fs->inodes_per_group = inodes_per_group;
    fs->inode_size = inode_size;
    fs->groups_count = (uint32_t)block_groups;
    fs->feature_compat = feature_compat;
    fs->feature_incompat = feature_incompat;
    fs->feature_ro_compat = feature_ro_compat;
    fs->filesystem_bytes = filesystem_bytes;
    fs->group_desc_offset = (uint64_t)(first_data_block + 1u) * block_size;

    if (fs->group_desc_offset > filesystem_bytes ||
        (uint64_t)fs->groups_count * EXT2_GROUP_DESC_SIZE >
            filesystem_bytes - fs->group_desc_offset) {
        klog(LOG_WARNING, "Ext2: group descriptor table exceeds filesystem bounds");
        ext2_release_context(fs);
        return (void*)0;
    }

    ext2_node_data_t* root_data = (ext2_node_data_t*)kmalloc(sizeof(*root_data));
    if (!root_data) {
        ext2_release_context(fs);
        return (void*)0;
    }
    if (ext2_read_inode(fs, EXT2_ROOT_INODE, &root_data->inode) != 0 ||
        (root_data->inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        kfree(root_data);
        ext2_release_context(fs);
        klog(LOG_WARNING, "Ext2: root inode is missing or invalid");
        return (void*)0;
    }
    root_data->fs = fs;
    root_data->inode_number = EXT2_ROOT_INODE;
    root_data->size = ext2_inode_size(fs, &root_data->inode);
    if (root_data->inode.i_flags & EXT2_INDEX_FL) {
        kfree(root_data);
        ext2_release_context(fs);
        klog(LOG_WARNING, "Ext2: indexed root directories are not supported");
        return (void*)0;
    }

    vfs_node_t* root = vfs_create_node(mount_point + 1, VFS_DIRECTORY);
    if (!root) {
        kfree(root_data);
        ext2_release_context(fs);
        return (void*)0;
    }
    root->ptr = root_data;
    root->size = root_data->size;
    root->permissions = root_data->inode.i_mode & 0777u;
    fs->node_count = 1;

    uint32_t ancestry[EXT2_MAX_DEPTH] = {0};
    if (ext2_build_directory(fs, root, root_data, 0, ancestry) != 0) {
        ext2_free_tree(root);
        ext2_release_context(fs);
        klog(LOG_WARNING, "Ext2: directory walk encountered malformed metadata; mount rejected");
        return (void*)0;
    }

    if (vfs_mount(mount_point, root) != 0) {
        ext2_free_tree(root);
        ext2_release_context(fs);
        klog(LOG_WARNING, "Ext2: VFS mount failed; filesystem remains unmounted");
        return (void*)0;
    }

    fs->active = 1;
    klog(LOG_INFO, "Ext2: mounted supported on-disk filesystem read-only; writes and indexed directories are disabled");
    return root;
}

/* Sample-only adapters are deliberately limited to the simulated RAM disk. */
static uint64_t ext2_demo_file_read(vfs_node_t* node, uint64_t offset,
                                    uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    ext2_demo_file_t* file = (ext2_demo_file_t*)node->ptr;
    if (!file->content || offset >= file->size || size == 0) return 0;
    uint64_t bytes = size;
    if (bytes > file->size - offset) bytes = file->size - offset;
    const char* src = file->content + offset;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = (uint8_t)src[i];
    return bytes;
}

static vfs_node_t* ext2_demo_dir_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) return (void*)0;
    vfs_node_t* child = node->first_child;
    while (child && index) { child = child->next_sibling; --index; }
    return child;
}

static vfs_node_t* ext2_mount_demo(block_device_t* dev, const char* mount_point) {
    if (!dev || !(dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
        (!strings_equal(mount_point, "/boot") && !strings_equal(mount_point, "/usr")) ||
        vfs_lookup(mount_point)) return (void*)0;

    const int is_boot = strings_equal(mount_point, "/boot");
    const char* file_name = is_boot ? "phoxtra.cfg" : "system.conf";
    const char* content = is_boot ? boot_cfg_text : sys_config_text;
    vfs_node_t* dir = vfs_create_node(mount_point + 1, VFS_DIRECTORY);
    if (!dir) return (void*)0;
    dir->readdir = ext2_demo_dir_readdir;

    ext2_demo_file_t* file = (ext2_demo_file_t*)kmalloc(sizeof(*file));
    if (!file) { kfree(dir); return (void*)0; }
    kstrncpy(file->name, file_name, (int)sizeof(file->name));
    file->content = content;
    file->size = is_boot ? sizeof(boot_cfg_text) - 1u : sizeof(sys_config_text) - 1u;

    vfs_node_t* node = vfs_create_node(file_name, VFS_FILE);
    if (!node) { kfree(file); kfree(dir); return (void*)0; }
    node->size = file->size;
    node->read = ext2_demo_file_read;
    node->ptr = file;
    node->parent = dir;
    dir->first_child = node;

    if (vfs_mount(mount_point, dir) != 0) {
        kfree(node);
        kfree(file);
        kfree(dir);
        klog(LOG_WARNING, "Ext2 demo adapter: VFS mount failed");
        return (void*)0;
    }

    klog(LOG_INFO, "Ext2 demo adapter mounted in-memory sample content; on-disk ext2 parsing is separate");
    return dir;
}

vfs_node_t* ext2_mount(block_device_t* dev, const char* mount_point) {
    if (!dev || !mount_point) return (void*)0;
    if (dev->flags & BLOCKDEV_FLAG_SIMULATED) return ext2_mount_demo(dev, mount_point);
    return ext2_mount_readonly(dev, mount_point);
}

void ext2_init(void) {
    klog(LOG_INFO, "Ext2 filesystem driver initialized; on-disk support is read-only");

    block_device_t* p1 = blockdev_get("ramdisk0p1");
    block_device_t* p2 = blockdev_get("ramdisk0p2");
    if (p1) (void)ext2_mount_demo(p1, "/boot");
    if (p2) (void)ext2_mount_demo(p2, "/usr");

    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; ++i) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (!dev || !(dev->flags & BLOCKDEV_FLAG_PARTITION) ||
            (dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
            !(dev->flags & BLOCKDEV_FLAG_READONLY)) continue;

        size_t name_len = 0;
        while (name_len < sizeof(dev->name) && dev->name[name_len]) ++name_len;
        if (!name_len || name_len >= sizeof(dev->name) || name_len > 37u) continue;
        char mount_path[40];
        mount_path[0] = '/';
        for (size_t n = 0; n < name_len; ++n) mount_path[n + 1u] = dev->name[n];
        mount_path[name_len + 1u] = '\0';
        (void)ext2_mount_readonly(dev, mount_path);
    }
}

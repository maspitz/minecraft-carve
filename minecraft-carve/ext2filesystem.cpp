#include "ext2filesystem.hpp"
#include <ext2fs/ext2fs.h>
#include <stdexcept>

namespace mcarve {

Ext2Filesystem::Ext2Filesystem(const std::string &name) {
    int flags = 0;      // open filesystem for reading only
    int superblock = 0; // use primary superblock
    int block_size = 0; // use superblock to determine block size

    errcode_t errval;

    initialize_ext2_error_table();

    errval = ext2fs_open(name.c_str(), flags, superblock, block_size,
                         unix_io_manager, &m_fs);
    if (errval) {
        com_err(name.c_str(), errval, "while opening filesystem");
        throw std::runtime_error("Could not open filesystem");
    }

    errval = ext2fs_read_block_bitmap(m_fs);
    if (errval) {
        com_err(name.c_str(), errval, "while reading block bitmaps");
        throw std::runtime_error("Could not read block bitmap");
    }
}

Ext2Filesystem::~Ext2Filesystem() { ext2fs_close(m_fs); }

bool Ext2Filesystem::block_is_used(uint64_t blk) {
    return ext2fs_test_block_bitmap2(m_fs->block_map, blk);
}

bool Ext2Filesystem::block_is_nonzero(uint64_t blk) {
    unsigned char buf[EXT2_MAX_BLOCK_SIZE];
    errcode_t errval;
    errval = io_channel_read_blk64(m_fs->io, blk, 1, buf);
    if (errval) {
        com_err("block is nonzero()", errval, "while reading block");
        throw std::runtime_error("Could not read block");
    }
    for (int i = 0; i < EXT2_MAX_BLOCK_SIZE; ++i) {
        if (buf[i] != 0) {
            return true;
        }
    }
    return false;
}

void Ext2Filesystem::read_block(uint64_t blk, std::vector<unsigned char> &data,
                                unsigned count) {
    data = std::vector<unsigned char>(m_fs->blocksize * count);
    read_block(blk, &data[0], count);
}

void Ext2Filesystem::read_block(uint64_t blk, void *data, unsigned count) {
    errcode_t errval = io_channel_read_blk64(m_fs->io, blk, count, data);
    if (errval) {
        com_err("Ext2Filesystem::read_block()", errval, "while reading block");
        throw std::runtime_error("Could not read block");
    }
}

} // namespace mcarve

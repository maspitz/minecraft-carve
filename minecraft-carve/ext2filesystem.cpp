#include <fstream>
#include <stdexcept>

#include <ext2fs/ext2fs.h>

#include "ext2filesystem.hpp"

namespace mcarve {

bool IdentifyExt2FS(const std::string &filename) {
    unsigned char buffer[2];
    std::ifstream file(filename);
    if (!file.is_open() || !file.seekg(1080, std::ios::beg) ||
        !file.read(reinterpret_cast<char *>(buffer), 2)) {
        return false;
    }

    return buffer[0] == 0x53 && buffer[1] == 0xef;
}

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

bool Ext2Filesystem::block_is_used(uint64_t blk) const {
    return ext2fs_test_block_bitmap2(m_fs->block_map, blk);
}

void Ext2Filesystem::read_block(uint64_t blk, std::vector<unsigned char> &data,
                                unsigned count) const {
    if (data.size() != count * m_fs->blocksize) {
        throw std::runtime_error(
            "Incorrect buffer size in Ext2Filesystem::read_block");
    }
    read_block(blk, data.data(), count);
}

void Ext2Filesystem::read_block(uint64_t blk, void *data,
                                unsigned count) const {
    errcode_t errval = io_channel_read_blk64(m_fs->io, blk, count, data);
    if (errval) {
        com_err("Ext2Filesystem::read_block()", errval, "while reading block");
        throw std::runtime_error("Could not read block");
    }
}

} // namespace mcarve

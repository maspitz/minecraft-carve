#ifndef BLOCKREADER_H_
#define BLOCKREADER_H_

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>

// for POSIX mmap
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ext2filesystem.hpp"

namespace mcarve {

const int BLOCKSIZE = 4096;
using BlockBuffer = std::array<unsigned char, BLOCKSIZE>;

//! Abstract base class providing interface for reading 4kB data blocks.
class BlockReader {
  public:
    virtual ~BlockReader() = default;

    //! Reads the block of the given index into a provided buffer.
    virtual void read_block(uint64_t blknum, BlockBuffer &buf) = 0;

    //! Returns the first valid block index.
    virtual uint64_t first_blknum() const = 0;

    //! Returns the total number of blocks.
    virtual uint64_t blocks_count() const = 0;

    //! In the context of filesystem data, indicates whether a block is marked
    //! as being used Otherwise, returns false.
    virtual bool is_allocated(uint64_t blknum) { return false; }
};

//! Reader of 4kB data blocks from an ext2 filesystem.
class Ext2BlockReader : public BlockReader {
  public:
    Ext2BlockReader(const std::string &filename) : e2fs(filename) {
        if (e2fs.blocksize() != BLOCKSIZE) {
            throw std::runtime_error(
                "This ext2/3/4 filesystem does not have 4kB blocks");
        }
    }

    void read_block(uint64_t blknum, BlockBuffer &buf) override {
        e2fs.read_block(blknum, buf.data(), 1);
    }

    uint64_t first_blknum() const override { return e2fs.first_data_block(); }

    uint64_t blocks_count() const override { return e2fs.blocks_count(); }

    bool is_allocated(uint64_t blknum) override {
        return e2fs.block_is_used(blknum);
    }

  private:
    Ext2Filesystem e2fs;
};

//! Reads 4k data blocks from any old file.
class FileBlockReader : public BlockReader {
  private:
    std::ifstream file;
    uint64_t totalBlocks;

  public:
    FileBlockReader(const std::string &filename) {
        file.open(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        } else {
            file.seekg(0, std::ios::end);
            totalBlocks = file.tellg() / BLOCKSIZE;
            file.seekg(0, std::ios::beg);
        }
    }

    void read_block(uint64_t blknum, BlockBuffer &buf) override {
        if (blknum >= totalBlocks) {
            throw std::runtime_error("Block number out of range: " +
                                     std::to_string(blknum));
        }

        file.seekg(blknum * BLOCKSIZE);
        file.read(reinterpret_cast<char *>(buf.data()), BLOCKSIZE);
    }

    uint64_t first_blknum() const override { return 0; }

    uint64_t blocks_count() const override { return totalBlocks; }
};

//! Reads 4k data blocks from any old file using memory mapped reads.
class MmapBlockReader : public BlockReader {
  private:
    int fd;
    uint64_t fileSize;
    uint64_t totalBlocks;
    unsigned char *fileData;

  public:
    MmapBlockReader(const std::string &filename) {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        fileSize = lseek(fd, 0, SEEK_END);
        totalBlocks = fileSize / BLOCKSIZE;
        fileData = static_cast<unsigned char *>(
            mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0));
        if (fileData == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to mmap file");
        }
    }

    ~MmapBlockReader() {
        if (fileData != MAP_FAILED) {
            munmap(fileData, fileSize);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    void read_block(uint64_t blknum, BlockBuffer &buf) override {
        if (blknum >= totalBlocks) {
            throw std::out_of_range("Block number out of range: " +
                                    std::to_string(blknum));
        }

        memcpy(buf.data(), fileData + blknum * BLOCKSIZE, BLOCKSIZE);
    }

    uint64_t first_blknum() const override { return 0; }

    uint64_t blocks_count() const override { return totalBlocks; }
};

} // namespace mcarve

#endif // BLOCKREADER_H_

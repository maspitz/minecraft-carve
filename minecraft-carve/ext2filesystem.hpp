#ifndef EXT2FILESYSTEM_H_
#define EXT2FILESYSTEM_H_

#include <cstdint>
#include <ext2fs/ext2fs.h>
#include <string>
#include <vector>

namespace mcarve {

class Ext2Filesystem {
  public:
    Ext2Filesystem(const std::string &name);
    ~Ext2Filesystem();

    bool block_is_used(uint64_t blk);
    void read_block(uint64_t blk, std::vector<unsigned char> &data,
                    unsigned count = 1);
    void read_block(uint64_t blk, void *data, unsigned count = 1);
    unsigned int blocksize() { return m_fs->blocksize; }
    uint32_t first_data_block() { return m_fs->super->s_first_data_block; }
    uint64_t blocks_count() { return ext2fs_blocks_count(m_fs->super); }

  private:
    ext2_filsys m_fs;

    unsigned char m_buf[EXT2_MAX_BLOCK_SIZE];
};

} // namespace mcarve

#endif // EXT2FILESYSTEM_H_

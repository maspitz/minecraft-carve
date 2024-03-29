#ifndef EXT2FILESYSTEM_H_
#define EXT2FILESYSTEM_H_

#include <cstdint>
#include <string>
#include <vector>

#include <ext2fs/ext2fs.h>

namespace mcarve {

bool IdentifyExt2FS(const std::string &filename);

class Ext2Filesystem {
  public:
    Ext2Filesystem(const std::string &name);
    ~Ext2Filesystem();

    bool block_is_used(uint64_t blk) const;
    void read_block(uint64_t blk, std::vector<unsigned char> &data,
                    unsigned count = 1) const;
    void read_block(uint64_t blk, void *data, unsigned count = 1) const;
    unsigned int blocksize() const { return m_fs->blocksize; }
    uint32_t first_data_block() const {
        return m_fs->super->s_first_data_block;
    }
    uint64_t blocks_count() const { return ext2fs_blocks_count(m_fs->super); }

  private:
    ext2_filsys m_fs;
};

} // namespace mcarve

#endif // EXT2FILESYSTEM_H_

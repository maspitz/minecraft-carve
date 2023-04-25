#ifndef EXT2FILESYSTEM_H_
#define EXT2FILESYSTEM_H_

#include <ext2fs/ext2fs.h>
#include <string>
#include <vector>

namespace mcarve {

class Ext2Filesystem {
  public:
    Ext2Filesystem(const std::string &name);
    ~Ext2Filesystem();

    bool block_is_used(blk64_t blk);
    bool block_is_nonzero(blk64_t blk);
    std::vector<unsigned char> read_block(blk64_t blk, unsigned count = 1);
    unsigned int blocksize() { return m_fs->blocksize; }

  private:
    ext2_filsys m_fs;

    unsigned char m_buf[EXT2_MAX_BLOCK_SIZE];
};

} // namespace mcarve

#endif // EXT2FILESYSTEM_H_

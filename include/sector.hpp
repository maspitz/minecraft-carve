// sector.hpp

#ifndef SECTOR_H_
#define SECTOR_H_

#include <bit>
#include <cstdint>
#include <ext2fs/ext2fs.h>
#include <span>
#include <vector>

#include "ext2filesystem.hpp"

namespace mcarve {

class Sector {
  public:
    // N_CHUNKS is the number of chunks that a region file can describe
    static constexpr int N_CHUNKS = 1024;

    // SECTOR_BYTES is the number of bytes in each sector of a region file
    static constexpr int SECTOR_BYTES = N_CHUNKS * sizeof(uint32_t);

    Sector() {}
    bool is_sparse() const;
    bool has_timestamps(uint32_t min_time, uint32_t max_time) const;
    bool has_offsets() const;
    bool has_offsets_bak() const;
    bool has_encoded_chunk() const;

    uint32_t get_timestamp(uint32_t chunk_index) const;
    uint32_t chunk_sector_offset(uint32_t chunk_index) const;
    uint8_t chunk_sector_length(uint32_t chunk_index) const;
    uint32_t encoded_chunk_bytelength() const;

    void read_sectors(Ext2Filesystem &fs, blk64_t blk, unsigned int n = 1);
    void read_sectors(Ext2Filesystem &fs, const std::vector<blk64_t> &blks);
    void extend_sectors(Ext2Filesystem &fs, blk64_t blk, unsigned int n = 1);
    void extend_sectors(Ext2Filesystem &fs, const std::vector<blk64_t> &blks);

    std::vector<uint8_t> get_chunk_nbt() const;

  private:
    std::span<const uint32_t> as_uint32() const {
        return std::span<const uint32_t>(
            reinterpret_cast<const uint32_t *>(m_bytes.data()),
            m_bytes.size() / sizeof(uint32_t));
    }
    std::vector<uint8_t> m_bytes;
    std::vector<blk64_t> m_blocks;
};
} // namespace mcarve
#endif // SECTOR_H_

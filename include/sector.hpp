// sector.hpp

#ifndef SECTOR_H_
#define SECTOR_H_

#include <vector>
#include <span>
#include <bit>
#include <cstdint>
#include <ext2fs/ext2fs.h>

#include "ext2filesystem.hpp"

namespace mcarve {

class Sector {
  public:
    static constexpr int N_CHUNKS = 1024;

        Sector() { }
    bool is_sparse() const;
    bool has_timestamps(uint32_t min_time, uint32_t max_time) const;
    bool has_offsets() const;
    bool has_offsets_bak() const;
    bool has_encoded_chunk() const;

    uint32_t get_timestamp(uint32_t chunk_index) const;
    uint32_t chunk_sector_offset(uint32_t chunk_index) const;
    uint8_t chunk_sector_length(uint32_t chunk_index) const;
    uint32_t encoded_chunk_bytelength() const;

    void read_sector(Ext2Filesystem &fs, blk64_t blk);

  private:
    std::span<const uint32_t> as_uint32() const {
        return std::span<const uint32_t>( reinterpret_cast<const uint32_t*>(m_bytes.data()),
                                    m_bytes.size() / sizeof(uint32_t));
    }
    std::vector<uint8_t> m_bytes;
};
} // namespace mcarve
#endif // SECTOR_H_

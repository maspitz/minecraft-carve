// sector.cpp

#include <algorithm>
#include <bitset>
#include <map>
#include <stdexcept>

#include "sector.hpp"

namespace mcarve {

bool Sector::is_sparse() const {
    constexpr int threshold = 10;
    const auto nonzero_count = std::count_if(m_bytes.begin(), m_bytes.end(),
                                             [](uint8_t x) { return x != 0; });
    return nonzero_count < threshold;
}

bool Sector::has_timestamps(uint32_t min_time, uint32_t max_time) const {
    bool has_timestamps = false;
    for (auto x : as_uint32()) {
        uint32_t timestamp = __builtin_bswap32(x);
        if (timestamp != 0) {
            has_timestamps = true;
            if (timestamp < min_time || timestamp > max_time) {
                return false;
            }
        }
    }

    return has_timestamps;
}

uint32_t Sector::get_timestamp(uint32_t chunk_index) const {
    return __builtin_bswap32(as_uint32()[chunk_index]);
}

uint32_t Sector::chunk_sector_offset(uint32_t chunk_index) const {
    return (__builtin_bswap32(as_uint32()[chunk_index]) >> 8) & 0xFFFFFF;
}

uint8_t Sector::chunk_sector_length(uint32_t chunk_index) const {
    return static_cast<uint8_t>(__builtin_bswap32(as_uint32()[chunk_index]) &
                                0xFF);
}

bool Sector::has_offsets() const {
    std::map<uint32_t, uint8_t> chunk_length;

    uint32_t offset_count = 0;

    for (uint32_t i = 0; i < Sector::N_CHUNKS; ++i) {
        auto length = chunk_sector_length(i);
        auto offset = chunk_sector_offset(i);

        // Reject candidate offset sectors with chunk offsets that are
        // unreasonably large.  Chunk sector offset > 0xffff corresponds to a
        // region file > 256 MB.
        if (offset > 0xffff) {
            return false;
        }
        if (length > 0) {
            chunk_length[chunk_sector_offset(i)] = length;
            offset_count++;
        }
    }

    // Reject candidate offset sectors that have fewer than four chunks.
    if (offset_count < 4) {
        return false;
    }

    // Reject candidate offset sectors with offsets inconsistent with chunk
    // lengths.
    uint32_t min_allowed_offset{2};

    for (auto [offset, length] : chunk_length) {
        if (length > 0 && offset < min_allowed_offset) {
            return false;
        }
        min_allowed_offset += length;
    }
    return true;
}

uint32_t Sector::encoded_chunk_bytelength() const {
    return __builtin_bswap32(as_uint32()[0]);
}

void Sector::read_sectors(Ext2Filesystem &fs, blk64_t blk, unsigned int n) {
    if (fs.blocksize() != SECTOR_BYTES) {
        throw std::runtime_error(
            "Filesystem's blocksize does not match region file sector size.");
    }
    if (m_bytes.size() != SECTOR_BYTES * n) {
        m_bytes = std::vector<uint8_t>(SECTOR_BYTES * n);
    }
    fs.read_block(blk, m_bytes.data(), n);
    m_blocks.clear();
    for (blk64_t b = blk; b < blk + n; ++b) {
        m_blocks.push_back(b);
    }
}

void Sector::read_sectors(Ext2Filesystem &fs,
                          const std::vector<blk64_t> &blks) {
    if (fs.blocksize() != SECTOR_BYTES) {
        throw std::runtime_error(
            "Filesystem's blocksize does not match region file sector size.");
    }
    if (m_bytes.size() != SECTOR_BYTES * blks.size()) {
        m_bytes = std::vector<uint8_t>(SECTOR_BYTES * blks.size());
    }
    for (int i = 0; i < blks.size(); ++i) {
        fs.read_block(blks[i], &m_bytes[i * SECTOR_BYTES], 1);
    }
    m_blocks = blks;
}

void Sector::extend_sectors(Ext2Filesystem &fs, blk64_t blk, unsigned int n) {
    if (fs.blocksize() != SECTOR_BYTES) {
        throw std::runtime_error(
            "Filesystem's blocksize does not match region file sector size.");
    }
    auto dst_idx = m_bytes.size();
    m_bytes.insert(m_bytes.end(), SECTOR_BYTES * n, 0);
    fs.read_block(blk, &m_bytes[dst_idx], n);
    for (blk64_t b = blk; b < blk + n; ++b) {
        m_blocks.push_back(b);
    }
}

void Sector::extend_sectors(Ext2Filesystem &fs,
                            const std::vector<blk64_t> &blks) {
    if (fs.blocksize() != SECTOR_BYTES) {
        throw std::runtime_error(
            "Filesystem's blocksize does not match region file sector size.");
    }
    auto dst_idx = m_bytes.size();
    m_bytes.insert(m_bytes.end(), SECTOR_BYTES * blks.size(), 0);
    for (int i = 0; i < blks.size(); ++i) {
        fs.read_block(blks[i], &m_bytes[dst_idx + i * SECTOR_BYTES], 1);
    }
    m_blocks.insert(m_blocks.end(), blks.begin(), blks.end());
}

namespace zlib_expected {
    constexpr uint8_t ZLIB_COMPRESSION = 0x02;

    constexpr uint8_t COMPRESSION_METHOD_DEFLATE = 0x08;
    constexpr uint8_t WINDOW_SIZE_32K = 0x70;
    constexpr uint8_t CMF = COMPRESSION_METHOD_DEFLATE | WINDOW_SIZE_32K;

    constexpr uint8_t FLEVEL_DEFAULT_ALGORITHM = 0x80;
    constexpr uint8_t FDICT_UNSET = 0x00;
    constexpr uint8_t FCHECK =
        31 - (CMF * 256 + FLEVEL_DEFAULT_ALGORITHM + FDICT_UNSET) % 31;
    constexpr uint8_t FLG = FLEVEL_DEFAULT_ALGORITHM | FDICT_UNSET | FCHECK;

    constexpr uint32_t ZLIB_CMF_FLG =
        (ZLIB_COMPRESSION << 24) | (CMF << 16) | (FLG << 8);
} // namespace zlib_expected

bool Sector::has_encoded_chunk() const {
    const uint32_t data_length = __builtin_bswap32(as_uint32()[0]);
    constexpr uint32_t MAX_DATA_LENGTH =
        (1 << 20); // Maximum possible encoded chunk length
    if (data_length > MAX_DATA_LENGTH) {
        return false;
    }

    // Upper byte encodes compression type, which should be ZLIB.
    // If compression type is ZLIB, then next two bytes encode CMF and FLG.
    const uint32_t comp_cmf_flg = __builtin_bswap32(as_uint32()[1]);
    if ((comp_cmf_flg & 0xFFFFFF00) != zlib_expected::ZLIB_CMF_FLG) {
        return false;
    }

    return true;
}

using bin1024 = std::bitset<1024>;
using bin64 = std::bitset<64>;

// unsigned long long
// block_fingerprint(const array<uint32_t, FIELDS_PER_BLOCK> &buffer) {
//     bin1024 population;
//     hash<bin1024> hash_fn;
//     for (size_t i = 0; i < FIELDS_PER_BLOCK; ++i) {

//         population[i] = (buffer[i] != 0);
//     }
//     bin64 x = hash_fn(population);
//     return x.to_ullong();
// }

} // namespace mcarve

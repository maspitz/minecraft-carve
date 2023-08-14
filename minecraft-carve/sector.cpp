// sector.cpp

#include <algorithm>
#include <map>
#include <span>
#include <stdexcept>

#include "sector.hpp"

namespace mcarve {

bool is_mostly_zero(const std::span<unsigned char> buffer) {
    std::span<const uint32_t> buf_u32(
        reinterpret_cast<const uint32_t *>(buffer.data()),
        buffer.size() / sizeof(uint32_t));
    constexpr int threshold = 10;
    int nonzero_count = std::count_if(buf_u32.begin(), buf_u32.end(),
                                      [](uint8_t x) { return x != 0; });
    return nonzero_count < threshold;
}

bool has_timestamps(const std::span<unsigned char> buffer, uint32_t min_time,
                    uint32_t max_time) {
    uint32_t bits(0);
    std::span<const uint32_t> buf_u32(
        reinterpret_cast<const uint32_t *>(buffer.data()),
        buffer.size() / sizeof(uint32_t));
    for (auto timestamp : buf_u32) {
        timestamp = __builtin_bswap32(timestamp);
        bits |= timestamp;
        if (timestamp != 0 && (timestamp < min_time || timestamp > max_time)) {
            return false;
        }
    }
    return bits != 0;
}

bool has_offsets(const std::span<unsigned char> buffer) {
    std::span<const uint32_t> buf_u32(
        reinterpret_cast<const uint32_t *>(buffer.data()),
        buffer.size() / sizeof(uint32_t));

    std::map<uint32_t, uint8_t> chunk_length;

    uint32_t offset_count = 0;

    for (uint32_t field : buf_u32) {
        field = __builtin_bswap32(field);
        uint8_t length = field & 0xff;
        uint32_t offset = field >> 8;

        // Reject candidate offset sectors with chunk offsets that are
        // unreasonably large.  Chunk sector offset > 0xffff corresponds to a
        // region file > 256 MB.
        if (offset > 0xffff) {
            return false;
        }
        if (chunk_length.contains(offset)) {
            return false;
        }
        if (length > 0) {
            chunk_length[offset] = length;
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

bool has_encoded_chunk(const std::span<unsigned char> buffer) {
    if (buffer.size() < 8) {
        return false;
    }
    std::span<const uint32_t> buf_u32(
        reinterpret_cast<const uint32_t *>(buffer.data()),
        buffer.size() / sizeof(uint32_t));

    const uint32_t data_length = __builtin_bswap32(buf_u32[0]);

    constexpr uint32_t MAX_DATA_LENGTH =
        (1 << 20); // Maximum possible encoded chunk length

    if (data_length > MAX_DATA_LENGTH) {
        return false;
    }

    // First byte COMP is 0x02 for compression type ZLIB.
    // Second byte CMF is 0x78.
    // Third byte FLG is 0x9c.
    const uint32_t COMP_CMF_FLG = 0x02789c00;

    return ((__builtin_bswap32(buf_u32[1]) & 0xFFFFFF00) == COMP_CMF_FLG);
}

} // namespace mcarve

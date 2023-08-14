/**
 * @file Sector.hpp
 * @brief Classifies candidate sector types in Minecraft file carving
 * @author Michael Spitznagel
 * @copyright Copyright 2023 Michael Spitznagel. Released under the GNU GPL 3
 * License.
 *
 * https://github.com/maspitz/minecraft-carve
 */

#ifndef SECTOR_H_
#define SECTOR_H_

#include <cstdint>
#include <span>

namespace mcarve {

//! Tests if a byte buffer has less than 10 nonzero 32-bit words.
bool is_mostly_zero(const std::span<unsigned char> buffer);

//! Tests if a byte buffer could be a big-endian 32-bit timestamp table.
bool has_timestamps(const std::span<unsigned char> buffer, uint32_t min_time,
                    uint32_t max_time);

//! Tests if a byte buffer could be a sector offset table.
bool has_offsets(const std::span<unsigned char> buffer);

//! Tests if a byte buffer could be the beginning of an encoded chunk.
bool has_encoded_chunk(const std::span<unsigned char> buffer);

} // namespace mcarve
#endif // SECTOR_H_

#include <array>
#include <bitset>
#include <cstdint> // for types uint32_t etc.
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ext2filesystem.hpp"
#include "nbt.hpp"
#include "pass1config.hpp"
#include "zstr.hpp"

using namespace std;
using namespace mcarve;

bool is_sparse_block(const std::vector<unsigned char> &blockdata) {
    unsigned int count_nonzero{0};
    for (auto data : blockdata) {
        count_nonzero += (data != 0);
    }
    return count_nonzero < 10;
}

bool is_timestamp_block(const std::vector<unsigned char> &blockdata,
                        uint32_t min_time, uint32_t max_time) {
    bool is_timestamp = false;
    const auto n_fields = blockdata.size() / sizeof(uint32_t);
    auto uint32data = reinterpret_cast<const uint32_t *>(blockdata.data());
    for (size_t i = 0; i < n_fields; ++i) {
        uint32_t timestamp = __builtin_bswap32(uint32data[i]);
        if (timestamp != 0) {
            is_timestamp = true;
            if (timestamp < min_time || timestamp > max_time) {
                is_timestamp = false;
                break;
            }
        }
    }
    return is_timestamp;
}

// NOTE: perhaps make Blockdata type.  Provide chunk_timestamp, chunk_length,
// etc. as methods of the type.  Maybe overload methods to provide access via
// index or (x,z). Blockdata should be initialized only with 4k of uint8_t or
// unsigned char. However, we also want access to NBT data, which requires
// multiple blocks to provide.  But this is a good starting point.

// chunk_index() provides the index number of a given (x, z) chunk's data in a
// region file header block. To use this index, interpret the header block data
// as an array of uint32_t[1024].
uint32_t chunk_index(uint32_t x, uint32_t z) {
    return ((x & 31) + (z & 31) * 32);
}

uint32_t chunk_timestamp(uint32_t chunk_index,
                         const std::vector<unsigned char> &blockdata) {
    return __builtin_bswap32(blockdata[chunk_index]);
}

// chunk_length() provides the length of the encoded chunk data in 4k sectors.
uint32_t chunk_length(uint32_t chunk_index,
                      const std::vector<unsigned char> &blockdata) {
    return __builtin_bswap32(blockdata[chunk_index]) >> 8 & 0xFFFFFF;
}

// chunk_length() provides the offset of the encoded chunk data in 4k sectors.
//
// The offsets and timestamps tables are located at offsets 0 and 1
// respectively, so the smallest possible valid chunk data offset is 2.
uint8_t chunk_offset(uint32_t chunk_index,
                     const std::vector<unsigned char> &blockdata) {
    return static_cast<uint8_t>(__builtin_bswap32(blockdata[chunk_index]) &
                                0xFF);
}

bool is_offset_block(const std::vector<unsigned char> &blockdata) {
    auto uint32data = reinterpret_cast<const uint32_t *>(blockdata.data());
    const auto n_fields = blockdata.size() / sizeof(uint32_t);
    for (size_t i = 0; i < n_fields; ++i) {
        uint32_t x = __builtin_bswap32(uint32data[i]);
        uint8_t upper_byte = static_cast<uint8_t>((x >> 24) & 0xFF);
        if (upper_byte & 0xFE) {
            return false;
        }
    }

    map<int32_t, uint8_t> chunk_length;
    for (size_t i = 0; i < n_fields; ++i) {
        uint32_t x = __builtin_bswap32(uint32data[i]);
        uint32_t offset = x >> 8 & 0xFFFFFF;
        uint8_t length = static_cast<uint8_t>(x & 0xFF);
        if (length > 0) {
            chunk_length[offset] = length;
        }
    }

    uint32_t calculated_offset{2};

    if (chunk_length.size() < 3) {
        return false;
    }
    int trials = 0;
    for (auto i : chunk_length) {
        trials++;
        uint32_t offset(i.first);
        auto length = i.second;
        if (offset < calculated_offset) {
            return false;
        }
        calculated_offset += length;
    }
    return true;
}

namespace zlib_expected {
constexpr uint8_t COMPRESSION_METHOD_DEFLATE = 0x08;
constexpr uint8_t WINDOW_SIZE_32K = 0x70;
constexpr uint8_t CMF = COMPRESSION_METHOD_DEFLATE | WINDOW_SIZE_32K;

constexpr uint8_t FLEVEL_DEFAULT_ALGORITHM = 0x80;
constexpr uint8_t FDICT_UNSET = 0x00;
constexpr uint8_t FCHECK =
    31 - (CMF * 256 + FLEVEL_DEFAULT_ALGORITHM + FDICT_UNSET) % 31;
constexpr uint8_t FLG = FLEVEL_DEFAULT_ALGORITHM | FDICT_UNSET | FCHECK;
} // namespace zlib_expected

bool is_chunk_header_block(const std::vector<unsigned char> &blockdata) {
    auto uint32data = reinterpret_cast<const uint32_t *>(blockdata.data());
    uint32_t data_length = __builtin_bswap32(uint32data[0]);
    // Require the chunk data length to be less than 1 MiB.
    if (data_length > (1 << 20)) {
        return false;
    }
    uint32_t x = __builtin_bswap32(uint32data[1]);
    uint8_t compression_type = static_cast<uint8_t>((x >> 24) & 0xFF);
    if (compression_type != 2) {
        return false;
    }
    uint8_t cmf = static_cast<uint8_t>((x >> 16) & 0xFF);
    uint8_t flg = static_cast<uint8_t>((x >> 8) & 0xFF);

    if (cmf != zlib_expected::CMF) {
        return false;
    }

    if (flg != zlib_expected::FLG) {
        return false;
    }

    // TODO: attempt decompression and recognition of chunk NBT data
    return true;
}

enum class Block { Offset, Timestamp, ChunkStart, ChunkCont };

using bin1024 = bitset<1024>;
using bin64 = bitset<64>;

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

int main(int argc, char *argv[]) {

    unique_ptr<Pass1Config> conf_ptr;
    try {
        conf_ptr = make_unique<Pass1Config>(argc, argv);
    } catch (exception &e) {
        cerr << argv[0] << ": " << e.what() << "\n";
        return 1;
    }

    Pass1Config &conf = *conf_ptr;

    Ext2Filesystem fs(conf.file_name());

    if (conf.verbose()) {
        cout << "Start Timestamp:\t" << conf.start_time() << "\n"
             << "Stop Timestamp:\t" << conf.stop_time() << "\n"
             << "Image file:\t" << conf.file_name() << "\n"
             << "Blocksize:\t" << fs.blocksize() << endl;
    }

    map<uint64_t, Block> blocks;
    vector<uint64_t> timestamp_offsets;
    vector<uint64_t> offset_offsets;
    vector<uint64_t> chunk_offsets;

    bool flag = false;
    for (blk64_t blk = fs.first_data_block(); blk < fs.blocks_count(); ++blk) {
        if (fs.block_is_used(blk)) {
            continue;
        }
        auto data = fs.read_block(blk);
        if (is_timestamp_block(data, conf.start_time(), conf.stop_time())) {
            std::cout << blk << ": timestamps\n";
            flag = true;
        }
        if (is_offset_block(data)) {
            std::cout << blk << ": offsets\n";
            flag = true;
        }
        if (flag && is_chunk_header_block(data)) {
            std::cout << blk << ": chunk header(s...)\n";
            flag = false;
        }
    }

    //     for (uint64_t offset = 0; offset < file_size; offset += BUFFER_SIZE)
    //     {

    //         infile.seekg(offset, ios::beg);
    //         infile.read(reinterpret_cast<char *>(buffer.data()),
    //         BUFFER_SIZE); if (is_sparse_block(buffer)) {
    //             continue;
    //         }
    //         if (is_offset_block(buffer)) {
    //             blocks[offset] = Block::Offset;
    //         } else if (is_timestamp_block(buffer, conf.start_time(),
    //                                       conf.stop_time())) {
    //             blocks[offset] = Block::Timestamp;
    //         } else if (is_chunk_header(buffer)) {
    //             blocks[offset] = Block::ChunkStart;
    //         }
    //     }

    //     const long MAX_CHUNK_SIZE = (1 << 20);
    //     std::array<char, MAX_CHUNK_SIZE> zzbuffer;
    //     // auto zzbuffer = std::make_unique<char[]>(1 << 20);

    //     for (auto b : blocks) {
    //         if (b.second != Block::ChunkStart) {
    //             continue;
    //         }
    //         uint64_t loc = b.first;
    //         infile.seekg(loc, ios::beg);
    //         uint32_t data_length;
    //         infile.read(reinterpret_cast<char *>(&data_length), 4);
    //         data_length = __builtin_bswap32(data_length);
    //         if (data_length > 1 << 20) {
    //             cerr << "At loc " << loc << ", Data length " << data_length
    //                  << " greater than " << (1 << 20) << ".  Exiting...";
    //             break;
    //         }

    //         uint8_t compression_type;
    //         infile.read(reinterpret_cast<char *>(&compression_type), 1);
    //         if (compression_type != 2) {
    //             cerr << "Compression type is not zlib.  Exiting...";
    //             break;
    //         }
    //         //        auto zbuffer = std::make_unique<char[]>(data_length);
    //         // infile.read(zzbuffer.get(), data_length);
    //         infile.read(reinterpret_cast<char *>(zzbuffer.data()),
    //         data_length);

    //         // Create a std::basic_stringbuf object and set its content to
    //         the
    //         // buffer

    //         if (1 == 1) {
    //             std::basic_stringbuf<char> input_buffer;
    //             input_buffer.sputn(reinterpret_cast<char *>(zzbuffer.data()),
    //                                infile.gcount());

    //             // Create a std::istringstream from the stringbuf
    //             std::basic_istream<char> input_stream(&input_buffer);

    //             try {
    //                 zstr::istream zinput_stream(input_stream);

    //                 nbt::NBT tags;
    //                 tags.decode(zinput_stream);
    //                 if (chunks_read < 2) {
    //                     std::cout << tags << std::endl;
    //                 }
    //             } catch (zstr::Exception &ze) {
    //                 cerr << "zinput_stream: Exception at offset" << loc << ":
    //                 "
    //                      << ze.what() << endl;
    //                 continue;
    //             }
    //         } else {

    //             std::istringstream input_stream(std::string(
    //                 reinterpret_cast<char *>(zzbuffer.data()),
    //                 infile.gcount()));
    //             try {
    //                 zstr::istream zinput_stream(input_stream);

    //                 // Pass the input stream to another function
    //                 nbt::NBT tags;
    //                 tags.decode(zinput_stream);
    //                 if (chunks_read < 2) {
    //                     std::cout << tags << std::endl;
    //                 }
    //             } catch (zstr::Exception &ze) {
    //                 cerr << "zinput_stream: Exception at offset" << loc << ":
    //                 "
    //                      << ze.what() << endl;
    //                 continue;
    //             }
    //         }

    //         for (auto data_offset = loc + BLOCK_SIZE;
    //              data_offset <= loc + data_length; data_offset += BLOCK_SIZE)
    //              {
    //             blocks[data_offset] = Block::ChunkCont;
    //         }
    //         chunks_read++;
    //     }
    //     map<Block, std::string> block_char{{Block::Offset, "O"},
    //                                        {Block::Timestamp, "T"},
    //                                        {Block::ChunkStart, "c"},
    //                                        {Block::ChunkCont, "d"}};

    //     if (conf.verbose()) {

    //         uint64_t prev_loc = 0;
    //         for (auto b : blocks) {
    //             uint64_t loc = b.first;
    //             if (loc != prev_loc + BUFFER_SIZE) {
    //                 cout << "\n" << loc << ":\t";
    //             }
    //             cout << block_char[b.second];
    //             prev_loc = loc;
    //         }
    //         cout << endl;
    //     }

    //     /*
    // multimap<unsigned long long, pair<string, uint64_t>> fprint;
    // infile.clear();
    // infile.seekg(0, ios::beg);
    // for (auto offset : offset_offsets) {
    //     if (!infile) {
    //         cerr << "!!" << endl;
    //         break;
    //     }
    //     infile.seekg(offset, ios::beg);
    //     infile.read(reinterpret_cast<char *>(buffer.data()), BUFFER_SIZE);
    //     fprint.insert(make_pair(block_fingerprint(buffer),
    //                             make_pair("offset   ", offset)));
    // }

    // infile.clear();
    // infile.seekg(0, ios::beg);
    // for (auto offset : timestamp_offsets) {
    //     infile.seekg(offset, ios::beg);
    //     infile.read(reinterpret_cast<char *>(buffer.data()), BUFFER_SIZE);
    //     fprint.insert(make_pair(block_fingerprint(buffer),
    //                             make_pair("timestamp", offset)));
    // }
    // for (auto fp : fprint) {

    //     cout << std::hex << fp.first << "\t" << std::dec << fp.second.second
    //          << "\t" << fp.second.first << "\n";
    // }
    // cout << endl;
    // */

    return 0;
}

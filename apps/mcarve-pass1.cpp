#include <iostream>
#include <map>
#include <vector>

#include "ext2filesystem.hpp"
//#include "nbt.hpp"
#include "pass1config.hpp"
#include "sector.hpp"
#include "zstr.hpp"

using namespace std;
using namespace mcarve;

uint32_t chunk_index(uint32_t x, uint32_t z) {
    return ((x & 31) + (z & 31) * 32);
}

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

    Sector sec;

    map<uint64_t, Sector::Role> blocks;
    vector<uint64_t> timestamp_offsets;
    vector<uint64_t> offset_offsets;
    vector<uint64_t> chunk_offsets;

    bool flag = false;
    auto max_blk = fs.blocks_count();
    if (max_blk > 400000) {
        max_blk = 400000;
    }
    for (blk64_t blk = fs.first_data_block(); blk < max_blk; ++blk) {

        if (fs.block_is_used(blk)) {
            continue;
        }
        sec.read_sector(fs, blk);
        if (sec.has_timestamps(conf.start_time(), conf.stop_time())) {
            std::cout << blk << ": timestamps\n";
            flag = true;
        }
        if (sec.has_offsets()) {
            std::cout << blk << ": offsets\n";
            flag = true;
        }

        if (flag && sec.has_encoded_chunk()) {
            std::cout << blk << ": chunk header(s...)\n";
            flag = false;
        }
    }
    //
    //     for (uint64_t offset = 0; offset < file_size; offset += BUFFER_SIZE)
    //     {

    //         infile.seekg(offset, ios::beg);
    //         infile.read(reinterpret_cast<char *>(buffer.data()),
    //         BUFFER_SIZE); if (is_sparse_block(buffer)) {
    //             continue;
    //         }
    //         if (is_offset_block(buffer)) {
    //             blocks[offset] = Sector::Role::Offset;
    //         } else if (is_timestamp_block(buffer, conf.start_time(),
    //                                       conf.stop_time())) {
    //             blocks[offset] = Sector::Role::Timestamp;
    //         } else if (is_chunk_header(buffer)) {
    //             blocks[offset] = Sector::Role::ChunkStart;
    //         }
    //     }

    //     const long MAX_CHUNK_SIZE = (1 << 20);
    //     std::array<char, MAX_CHUNK_SIZE> zzbuffer;
    //     // auto zzbuffer = std::make_unique<char[]>(1 << 20);

    //     for (auto b : blocks) {
    //         if (b.second != Sector::Role::ChunkStart) {
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
    //             blocks[data_offset] = Sector::Role::ChunkCont;
    //         }
    //         chunks_read++;
    //     }
    //     map<Block, std::string> block_char{{Sector::Role::Offset, "O"},
    //                                        {Sector::Role::Timestamp, "T"},
    //                                        {Sector::Role::ChunkStart, "c"},
    //                                        {Sector::Role::ChunkCont, "d"}};

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

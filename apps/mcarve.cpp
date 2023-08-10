#include <iostream>
#include <map>
#include <vector>

#include "ext2filesystem.hpp"
#include "mcarve-config.hpp"
#include "sector.hpp"

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

    vector<uint64_t> timestamp_offsets;
    vector<uint64_t> offset_offsets;
    vector<uint64_t> chunk_offsets;

    auto max_blk = fs.blocks_count();
    if (max_blk > 400000) {
        max_blk = 400000;
    }
    vector<uint8_t> sector_buffer(4096);
    vector<uint8_t> tiny_buffer(16);
    int chunk_header_count = 0;
    for (blk64_t blk = fs.first_data_block(); blk < max_blk; ++blk) {

        if (fs.block_is_used(blk)) {
            continue;
        }
        fs.read_block(blk, sector_buffer.data(), 1);
        if (has_timestamps(sector_buffer, conf.start_time(),
                           conf.stop_time())) {
            if (chunk_header_count > 0) {
                std::cout << "[" << chunk_header_count << "]\n";
                chunk_header_count = 0;
            }
            std::cout << blk << ": timestamps\n";
        }
        if (has_offsets(sector_buffer)) {
            if (chunk_header_count > 0) {
                std::cout << "[" << chunk_header_count << "]\n";
                chunk_header_count = 0;
            }
            std::cout << blk << ": offsets\n";
        }

        if (has_encoded_chunk(sector_buffer)) {
            if (chunk_header_count == 0) {
                std::cout << blk << ": chunk headers: ";
            }
            chunk_header_count++;
        }
    }
    if (chunk_header_count > 0) {
        std::cout << "[" << chunk_header_count << "]\n";
        chunk_header_count = 0;
    }

    return 0;
}

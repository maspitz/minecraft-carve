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
        sec.read_sectors(fs, blk);
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

    return 0;
}

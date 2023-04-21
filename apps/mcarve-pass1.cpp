#include <string>
#include <iostream>
#include <ctime>
#include <stdexcept>
#include <cstdint>  // for types uint32_t etc.
#include <vector>

#include "pass1config.hpp"

using namespace std;
using namespace mcarve;

int main(int argc, char *argv[]) {

    const uint64_t BLOCK_SIZE = 4096;

    Pass1Config conf(argc, argv);

    ifstream &infile = conf.in_stream();

    infile.seekg(0, ios::end);
    uint64_t file_size = infile.tellg();
    if (file_size % BLOCK_SIZE != 0) {
        cerr << "Warning: image file size is not padded to align with block size." << endl;
    }
    infile.seekg(0, ios::beg);

    if (conf.verbose()) {
    	cout << "Start Timestamp: " << conf.start_time() << "\n"
             << "Stop Timestamp:  " << conf.stop_time() << "\n"
             << "Opened image file " << conf.file_name() << "\n"
             << "Contains " << file_size / 4096 << " blocks" << endl;
    }

    constexpr auto FIELD_SIZE = sizeof(uint32_t);
    constexpr auto FIELDS_PER_BLOCK = BLOCK_SIZE / FIELD_SIZE;
    uint32_t buffer[FIELDS_PER_BLOCK];

    vector<uint64_t> timestamp_offsets;

    for (uint64_t offset = 0; offset < file_size; offset += BLOCK_SIZE) {
        infile.seekg(offset, ios::beg);
        infile.read((char*)buffer, BLOCK_SIZE);

        bool is_timestamp = false;
        for (uint64_t i = 0; i < FIELDS_PER_BLOCK; ++i) {

            uint32_t timestamp = __builtin_bswap32(buffer[i]);
            if (timestamp != 0) {
                is_timestamp = true;
                if (timestamp < conf.start_time() || timestamp > conf.stop_time()) {
                    is_timestamp = false;
                    break;
                }
            }
        }

        if (is_timestamp) {
            timestamp_offsets.push_back(offset);
        }
    }
    cout << "Found " << timestamp_offsets.size()
         << " candidate timestamp blocks at offsets:\n\t";
    for(auto offset: timestamp_offsets) {
         cout << offset << " ";
    }
    cout << endl;

    return 0;
}

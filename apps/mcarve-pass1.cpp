#include <string>
#include <iostream>
#include <ctime>
#include <stdexcept>
#include <cstdint>  // for types uint32_t etc.
#include <array>
#include <vector>
#include <memory>
#include <map>

#include "pass1config.hpp"

using namespace std;
using namespace mcarve;

const uint64_t BLOCK_SIZE = 4096;
constexpr auto FIELD_SIZE = sizeof(uint32_t);
constexpr auto FIELDS_PER_BLOCK = BLOCK_SIZE / FIELD_SIZE;

bool is_sparse_block(array<uint32_t, FIELDS_PER_BLOCK>& buffer) {
    int count_nonzero{0};
    for (size_t i = 0; i < FIELDS_PER_BLOCK; ++i) {
        count_nonzero += (buffer[i] != 0);
    }
    return count_nonzero < 10;
}

bool is_timestamp_block(array<uint32_t, FIELDS_PER_BLOCK>& buffer, uint32_t min_time, uint32_t max_time) {
    bool is_timestamp = false;
    for (size_t i = 0; i < FIELDS_PER_BLOCK; ++i) {
        uint32_t timestamp = __builtin_bswap32(buffer[i]);
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

bool is_offset_block(array<uint32_t, FIELDS_PER_BLOCK>& buffer) {
    bool is_offset = false;
    for (size_t i = 0; i < FIELDS_PER_BLOCK; ++i) {
        uint8_t upper_byte = static_cast<uint8_t>((buffer[i] >> 24) & 0xFF);
        if (upper_byte & 0xFE) {
            return false;
        }
    }
    map<int32_t, uint8_t> chunk_length;
    for (size_t i = 0; i < FIELDS_PER_BLOCK; ++i) {
        uint32_t x = __builtin_bswap32(buffer[i]);
        uint32_t offset = x >> 8 & 0xFFFFFF;
        uint32_t length = x & 0xFF;
        if (length > 0) {
            chunk_length[offset] = length;
        }
    }

    uint32_t calculated_offset{2};


    if (chunk_length.size() < 3) {
        return false;
    }
    cerr << "chunk_length.size() = " << chunk_length.size() << endl;
    for (auto i : chunk_length) {
        uint32_t offset(i.first);
        uint8_t length(i.second);
        if (offset != calculated_offset) {
            return false;
        }
        calculated_offset += length;
    }
    cerr << chunk_length.size() << " ";
    return true;
}

int main(int argc, char *argv[]) {

    unique_ptr<Pass1Config> conf_ptr;
    try {
        conf_ptr = make_unique<Pass1Config>(argc, argv);
    }
    catch (exception& e) {
        cerr << argv[0] << ": " << e.what() << "\n";
        return 1;
    }

    Pass1Config& conf = *conf_ptr;
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

    array<uint32_t, FIELDS_PER_BLOCK> buffer;

    vector<uint64_t> timestamp_offsets;
    vector<uint64_t> offset_offsets;

    const size_t BUFFER_SIZE = buffer.size() * sizeof(uint32_t);

    for (uint64_t offset = 0; offset < file_size; offset += BUFFER_SIZE) {
        infile.seekg(offset, ios::beg);
        infile.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE);
        if (is_sparse_block(buffer)) {
            continue;
        }
        if (is_timestamp_block(buffer, conf.start_time(), conf.stop_time())) {
            timestamp_offsets.push_back(offset);
        }
        if (is_offset_block(buffer)) {
            offset_offsets.push_back(offset);
        }
    }

    cout << "Found " << timestamp_offsets.size()
         << " candidate timestamp blocks at offsets:\n\t";
    for(auto offset: timestamp_offsets) {
         cout << offset << " ";
    }
    cout << "\n";

    cout << "Found " << offset_offsets.size()
         << " candidate offset blocks at offsets:\n\t";
    for(auto offset: offset_offsets) {
         cout << offset << " ";
    }
    cout << "\n";

    return 0;
}

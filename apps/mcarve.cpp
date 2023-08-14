#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>

#include "CLI11.hpp"

#include "ext2filesystem.hpp"
#include "sector.hpp"

#include "BlockReader.hpp"

using namespace mcarve;

time_t parse_time(const std::string &timestr) {
    struct tm tm = {};
    char *ptr = strptime(timestr.c_str(), "%Y-%m-%d", &tm);
    if (ptr == nullptr || *ptr != '\0') {
        throw std::runtime_error("Failed to parse timestamp: " + timestr);
    }
    return mktime(&tm);
}

time_t current_time() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    return mktime(&tm);
}

int main(int argc, char *argv[]) {

    CLI::App app{"Scan filesystem image for minecraft data"};

    struct {
        std::string filename;
        uint32_t start_time;
        uint32_t stop_time;
        bool verbose;
    } config;

    config.verbose = false;
    app.add_option("-f,--file,file", config.filename, "Image file to be carved")
        ->required()
        ->check(CLI::ExistingFile);

    std::string start_timestr("2008-01-01");
    app.add_option("--start", start_timestr,
                   "Minimum accepted timestamp (YYYY-mm-dd)")
        ->default_str("2008-01-01");

    std::string stop_timestr = "current_time";
    app.add_option("--stop", stop_timestr,
                   "Maximum accepted timestamp (YYYY-mm-dd)")
        ->default_str("current_time");
    app.add_flag("-v,--verbose", config.verbose, "Print verbose output");

    time_t start_time;
    time_t stop_time;

    start_time = parse_time(start_timestr);
    if (stop_timestr == "current_time") {
        stop_time = current_time();
    } else {
        stop_time = parse_time(stop_timestr);
    }

    config.start_time = start_time;
    config.stop_time = stop_time;

    CLI11_PARSE(app, argc, argv);

    std::unique_ptr<BlockReader> reader;
    if (IdentifyExt2FS(config.filename)) {
        reader = std::make_unique<Ext2BlockReader>(config.filename);
    } else {
        reader = std::make_unique<FileBlockReader>(config.filename);
    }

    std::vector<uint64_t> timestamp_offsets;
    std::vector<uint64_t> offset_offsets;
    std::vector<uint64_t> chunk_offsets;

    auto max_blk = reader->blocks_count();
    if (max_blk > 400000) {
        max_blk = 400000;
    }
    BlockBuffer sector_buffer;

    int chunk_header_count = 0;
    for (blk64_t blk = reader->first_blknum(); blk < max_blk; ++blk) {
        if (reader->is_allocated(blk)) {
            continue;
        }
        reader->read_block(blk, sector_buffer);
        if (has_timestamps(sector_buffer, config.start_time,
                           config.stop_time)) {
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

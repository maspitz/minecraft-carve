#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "CLI11.hpp"

#include "ext2filesystem.hpp"

using namespace mcarve;

int main(int argc, char *argv[]) {

    CLI::App app{
        "Dump unused, nonzero data blocks from ext2/3/4 filesystem image"};

    struct {
        std::string input, output, table;
        bool verbose;
    } conf;

    app.add_option("-i,--input", conf.input, "Ext2/3/4 filesystem image")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-o,--output", conf.output, "Unused block data output");
    app.add_option("-t,--table", conf.table, "Unused block id output");
    app.add_flag("-v,--verbose", conf.verbose, "Print verbose output");

    CLI11_PARSE(app, argc, argv);

    if (conf.output == "" && conf.table == "") {
        std::cerr << argv[0]
                  << ": You must specify an output mode via --output, --table, "
                     "or both."
                  << std::endl;
        return EXIT_FAILURE;
    }

    Ext2Filesystem fs(conf.input);

    std::ofstream data_output;
    std::ofstream id_output;
    bool writing_blocks = false;
    bool writing_ids = false;

    if (conf.output != "") {
        writing_blocks = true;
        data_output.open(conf.output, std::ios::binary);
        if (!data_output.is_open()) {
            std::cerr << argv[0] << ": Couldn't open block data output file."
                      << std::endl;
            return 1;
        }
    }

    if (conf.table != "") {
        writing_ids = true;
        id_output.open(conf.table, std::ios::binary);
        if (!id_output.is_open()) {
            std::cerr << argv[0] << ": Couldn't open block id output file."
                      << std::endl;
            return 1;
        }
    }

    std::vector<char> block_buffer(4096);

    auto max_blk = fs.blocks_count();

    for (uint64_t blk = fs.first_data_block(); blk < max_blk; ++blk) {
        if (fs.block_is_used(blk)) {
            continue;
        }
        fs.read_block(blk, block_buffer.data(), 1);

        bool allZeroes = std::all_of(block_buffer.begin(), block_buffer.end(),
                                     [](char c) { return c == 0; });
        if (allZeroes) {
            continue;
        }

        if (writing_ids) {
            id_output.write(reinterpret_cast<const char *>(&blk), sizeof(blk));
        }

        if (writing_blocks) {
            data_output.write(block_buffer.data(), block_buffer.size());
        }

        bool errcheck = ((blk % 1024) == 0);
        if (errcheck) {
            if (id_output.bad() || data_output.bad()) {
                std::cerr << argv[0] << ": Write error" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    id_output.flush();
    data_output.flush();
    if (id_output.bad() || data_output.bad()) {
        std::cerr << argv[0] << ": Write error" << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}

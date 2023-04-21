#include "CLI11.hpp"
#include <string>
#include <iostream>
#include <ctime>
#include <stdexcept>
#include <cstdint>  // for types uint32_t etc.
#include <vector>

using namespace std;

time_t parse_time(const std::string& timestr) {
    struct tm tm = {};
    char* ptr = strptime(timestr.c_str(), "%Y-%m-%d", &tm);
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
    CLI::App app{"Minecraft file carver pass-1"};
    bool verbose{false};

    std::string filename = "default";
    std::string start_timestr = "2008-01-01";
    std::string stop_timestr = "default";
    app.add_option("-f,--file", filename, "Image file to be carved")
	    ->required()
	    ->check(CLI::ExistingFile);
    app.add_option("--start", start_timestr, "Minimum accepted timestamp (YYYY-mm-dd)");
    app.add_option("--stop", stop_timestr, "Maximum accepted timestamp (YYYY-mm-dd)");
    app.add_flag("-v,--verbose", verbose, "Print verbose output");

    CLI11_PARSE(app, argc, argv);

    time_t start_time;
    time_t stop_time;
    try {
    	start_time = parse_time(start_timestr);
    	if (stop_timestr == "default") {
		    stop_time = current_time();
	    } else {
		    stop_time = parse_time(stop_timestr);
	    }
    } catch (std::runtime_error e) {
	    std::cerr << e.what() << std::endl;
	    return 1;
    }

    if (verbose) {
    	std::cout << "Start Timestamp: " << start_time << "\n"
		<< "Stop Timestamp:  " << stop_time << std::endl;
    }

    const uint64_t BLOCK_SIZE = 4096;

    ifstream infile(filename, ios::in | ios::binary);
    if (!infile) {
        cerr << "Error: Could not open file " << filename << endl;
        return 1;
    }

    infile.seekg(0, ios::end);
    uint64_t file_size = infile.tellg();
    if (file_size % BLOCK_SIZE != 0) {
        cerr << "Warning: image file size is not padded to align with block size." << endl;
    }
    infile.seekg(0, ios::beg);
    if (verbose) {
        cout << "Opened image file " << filename << "\n";
        cout << "Contains " << file_size / 4096 << " blocks" << endl;
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
                if (timestamp < start_time || timestamp > stop_time) {
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

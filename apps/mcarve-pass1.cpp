#include "CLI11.hpp"
#include <string>
#include <iostream>
#include <ctime>
#include <stdexcept>

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

    return 0;
}

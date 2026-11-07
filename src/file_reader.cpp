#include "file_reader.hpp"
#include <iostream>
#include <sstream>

FileReader::FileReader() {}

// return 1 on successful read, outputs to string document
bool FileReader::dumpFile(std::string filename) {
    readDocument = false;
    std::ifstream File;  // file input stream
    std::ostringstream ss;  // string accumulation buffer

    File.open(filename);
    if (File.is_open()) {
        ss << File.rdbuf();  // dump to buffer
        document = ss.str(); // format as string
        File.close();

        readDocument = true;
    } else {
        std::cout << "ERROR: File Failed to Open\n";
    }
    return readDocument;
}
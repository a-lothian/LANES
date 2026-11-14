#include "file_handler.hpp"

#include <fstream>
#include <sstream>

bool FileHandler::readFile(const std::string& path) {
    document.clear();
    error_.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        error_ = "cannot read '" + path + "'";
        return false;
    }

    std::ostringstream buffer;  // string accumulation buffer
    buffer << file.rdbuf();     // dump the whole file into it
    document = buffer.str();

    return true;
}

bool FileHandler::writeFile(const std::string& path, const std::string& contents) {
    error_.clear();

    std::ofstream file(path);
    if (!file.is_open()) {
        error_ = "cannot write '" + path + "'";
        return false;
    }

    file << contents;
    file.close();

    if (file.fail()) {
        error_ = "failed while writing '" + path + "'";
        return false;
    }

    return true;
}

const std::string& FileHandler::getError() const {
    return error_;
}

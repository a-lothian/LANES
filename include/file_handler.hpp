#ifndef LANES_FILE_HANDLER_HPP
#define LANES_FILE_HANDLER_HPP

#include <string>

// reads whole files into a string and writes strings out to files
class FileHandler {
   public:
    // contents of the most recent successful readFile (empty after a failed read)
    std::string document;

    // reads the file at `path` into `document`; returns false on failure and sets getError()
    bool readFile(const std::string& path);

    // writes `contents` to the file at `path`, replacing any existing file;
    // returns false on failure and sets getError()
    bool writeFile(const std::string& path, const std::string& contents);

    // describes the most recent failure, empty if the last operation succeeded
    const std::string& getError() const;

   private:
    std::string error_;
};

#endif  // LANES_FILE_HANDLER_HPP

#ifndef FILE_READ_HPP
#define FILE_READ_HPP

#include <string>
#include <fstream>

class FileReader {
    public:
    std::string document;

    FileReader();
    bool dumpFile(std::string file);

   private:
    std::string filename;
    bool readDocument;

};

#endif
#ifndef FILE_READ_HPP
#define FILE_READ_HPP

#include <string>
#include <fstream>

class FileReader {
    public:
    std::string document;
    
    FileReader(std::string file);
    bool dumpFile();

    private:
    std::string filename;
    bool readDocument;

};

#endif
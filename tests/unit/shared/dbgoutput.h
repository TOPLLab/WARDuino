#pragma once
#include <fstream>

#include "../../src/Debug/debugger.h"

class DBGOutput {
   private:
    Debugger* debugger{};

    char filename[250] = {"/tmp/warduinounittestXXXXXX"};
    int fd{-1};
    FILE* file{nullptr};
    std::ifstream stream;

    std::vector<std::string*> linesRead;
    bool openTmpFile();
    void closeTmpFile();

   protected:
   public:
    DBGOutput(Debugger* t_debugger);
    ~DBGOutput();

    bool open();
    std::string* getLine();
    bool getJSONReply(nlohmann::basic_json<>* dest);
    void appendReadLines(std::string* s);
};

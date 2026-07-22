#pragma once
#include "dbgoutput.h"

#include "../../src/Interrupts/interrupt_response.h"
#include "serialisation.h"

bool DBGOutput::openTmpFile() {
    this->fd = mkstemp(filename);
    if (fd == -1) {
        return false;
    }
    this->file = fdopen(fd, "r+");

    // open stream
    this->stream.open(this->filename);
    return true;
}

void DBGOutput::closeTmpFile() {
    close(fd);
    remove(filename);
    stream.close();
}

DBGOutput::DBGOutput(Debugger* t_debugger) : debugger{t_debugger} {}

DBGOutput::~DBGOutput() {
    this->closeTmpFile();
    for (int i = 0; i < this->linesRead.size(); i++) {
        delete this->linesRead[i];
    }
    this->linesRead.clear();
}

bool DBGOutput::open() {
    if (!this->openTmpFile()) return false;
    debugger->channel = new Sink(file);
    return true;
}

std::string* DBGOutput::getLine() {
    if (stream) {
        std::string* line = new std::string;
        std::getline(stream, *line);
        linesRead.push_back(line);
        return line;
    }
    return nullptr;
}

void DBGOutput::appendReadLines(std::string* s) {
    std::string concat;
    uint32_t stringIdx = 0;
    for (const auto& str : this->linesRead) {
        concat += "line ";
        concat += std::to_string(stringIdx++);
        concat += ": ";
        concat += *str;
        concat += "\n";
    }
    s->append(concat);
}

bool DBGOutput::getJSONReply(nlohmann::basic_json<>* dest) {
    std::string* line{};
    while ((line = this->getLine()) != nullptr) {
        try {
            *dest = nlohmann::json::parse(line->c_str());
            return true;
        } catch (const nlohmann::detail::parse_error& e) {
        }
    }
    return false;
}

void createReplyMessage(std::string& msg, InterruptTypes interrupt_nr,
                        const uint8_t response_type, uint8_t id,
                        uint8_t error_code) {
    std::string interruptHex{};
    Serialiser::uint8ToHexString(interrupt_nr, interruptHex);

    std::string idHex{};
    Serialiser::uint8ToHexString(id, idHex);

    std::string kindHex{};
    Serialiser::uint8ToHexString(response_type, kindHex);

    msg = R"({"interrupt":")" + interruptHex + R"(","kind":")" + kindHex +
          R"(","id":")" + idHex + "\"";
    if (error_code != NO_ERROR) {
        std::string errStr{};
        Serialiser::uint8ToHexString(error_code, errStr);
        msg += R"(,"error_code":")" + errStr + "\"";
    }

    msg += "}";
}

void createSuccessReplyMessage(std::string& msg, InterruptTypes interrupt_nr,
                               uint8_t id) {
    createReplyMessage(msg, interrupt_nr, INTERRUPT_RESPONSE_TYPE_SUCCESS, id,
                       NO_ERROR);
}

void createFailureReplyMessage(std::string& msg, InterruptTypes interrupt_nr,
                               uint8_t id, uint8_t error_code) {
    createReplyMessage(msg, interrupt_nr, INTERRUPT_RESPONSE_TYPE_ERROR, id,
                       error_code);
}

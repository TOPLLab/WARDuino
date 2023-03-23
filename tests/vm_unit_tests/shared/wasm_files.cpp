#pragma once
#include "wasm_files.h"
#include <tuple>

#define MAX_MODULE_SIZE (64 * 1024 * 1024)
const char* ERR_FILE_OPEN = "cannot open file";
const char* ERR_FILE_SIZE = "file is too large";
const char* ERR_FILE_MEMORY = "not enough memory for wasm binary";
const char* ERR_FILE_READ = "could not read file";

std::tuple<uint8_t*, unsigned int, const char*> Load_WASM(const char *file_name) {
    uint8_t *wasm;
    unsigned int file_size;

    FILE *file = fopen(file_name, "rb");
    if (!file)
        return {nullptr, 0, ERR_FILE_OPEN};

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size > MAX_MODULE_SIZE){
        fclose(file);
        return {nullptr, 0, ERR_FILE_SIZE};
    }

    wasm = (uint8_t *)malloc(file_size);
    if (!wasm) {
        fclose(file);
        return {nullptr, 0, ERR_FILE_MEMORY};
    }

    if (fread(wasm, 1, file_size, file) != file_size) {
        fclose(file);
        return {nullptr, 0, ERR_FILE_READ};
    }

    fclose(file);
    file = nullptr;

    return {wasm, file_size, nullptr};
}



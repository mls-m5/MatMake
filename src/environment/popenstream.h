//! Copyright Mattias Larsson Sköld 2020

#pragma once

#include "environment/files.h" // for popen macros
#include "main/merror.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>

class POpenStream : public std::istream {
private:
    struct POpenStreamBuf : public std::streambuf {
        POpenStreamBuf(std::string command, bool captureStdErr = false)
            : pfile(popen((command + (captureStdErr ? " 2>&1" : "")).c_str(),
                          "r")) {
            if (!pfile) {
                throw std::runtime_error("could not run command " + command);
            }
        }

        std::streambuf::int_type underflow() override {
            if (!pfile) {
                return std::char_traits<char>::eof();
            }

            if (fgets(buffer.data(), size, pfile)) {
                auto len = strlen(buffer.data());
                setg(buffer.data(), buffer.data(), buffer.data() + len);
                return std::char_traits<char>::to_int_type(*this->gptr());
            }
            else {
                pfile = nullptr;
                return std::char_traits<char>::eof();
            }
        }

        ~POpenStreamBuf() {
            if (pfile) {
                pclose(pfile);
            }
        }

        static constexpr size_t size = 1024 * 10;
        std::array<char, size> buffer;

        FILE *pfile;
    };

public:
    POpenStream(std::string command)
        : buffer(command) {
        rdbuf(&buffer);
    }

    POpenStreamBuf buffer;
};

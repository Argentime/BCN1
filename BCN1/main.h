#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <locale.h>
#include <stdbool.h>
#include <limits>

struct FrameInfo {
    uint8_t address;
    uint8_t control;
    uint8_t sequence;
    uint8_t variant;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> raw_frame;
};

std::vector<uint8_t> build_information_field(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant, const std::vector<uint8_t>& payload);
void parse_information_field(const std::vector<uint8_t>& info, FrameInfo& frame);
std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data);
std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data);
bool write_to_port(HANDLE hComm, const std::vector<uint8_t>& data);
bool read_from_port(HANDLE hComm, std::vector<uint8_t>& buffer);
bool send_string_as_frame(HANDLE hComm, 
    const std::string& message, 
    uint8_t& sequence,
    uint8_t address, 
    uint8_t control, 
    uint8_t variant);
bool receive_frame(HANDLE hComm);
void print_frame_info(const std::vector<uint8_t>& raw_frame);

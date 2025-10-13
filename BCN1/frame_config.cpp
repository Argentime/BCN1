#include <wtypes.h>
#include <iostream>
#include <vector>
#include "frame_config.h"

std::vector<uint8_t> build_information_field(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> info;
    info.push_back(address);
    info.push_back(control);
    info.push_back(sequence);
    info.push_back(variant);
    info.insert(info.end(), payload.begin(), payload.end());
    return info;
}

void parse_information_field(const std::vector<uint8_t>& info, FrameInfo& frame) {
    if (info.size() < 4) return;
    frame.address = info[0];
    frame.control = info[1];
    frame.sequence = info[2];
    frame.variant = info[3];
    // payload и FCS будут отделены позже
}

std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data) {
    const uint8_t ESC = 0x7D;
    std::vector<uint8_t> stuffed;
    for (uint8_t byte : data) {
        if (byte == FLAG || byte == ESC) {
            stuffed.push_back(ESC);
            stuffed.push_back(byte ^ 0x20);
        }
        else {
            stuffed.push_back(byte);
        }
    }
    stuffed.insert(stuffed.begin(), FLAG);
    stuffed.push_back(FLAG);
    return stuffed;
}

std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data) {
    const uint8_t ESC = 0x7D;
    std::vector<uint8_t> unstuffed;
    if (data.size() < 2) return unstuffed;
    for (size_t i = 1; i < data.size() - 1; i++) {
        if (data[i] == ESC && i + 1 < data.size() - 1) {
            unstuffed.push_back(data[i + 1] ^ 0x20);
            i++;
        }
        else {
            unstuffed.push_back(data[i]);
        }
    }
    return unstuffed;
}

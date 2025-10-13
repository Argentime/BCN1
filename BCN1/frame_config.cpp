#include <wtypes.h>
#include <iostream>
#include <vector>
#include "frame_config.h"
#include "hamming.h"

std::vector<uint8_t> build_information_field_with_fcs_and_length(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& encoded_payload, uint16_t orig_length, uint16_t fcs) {

    std::vector<uint8_t> info;
    info.push_back(address);
    info.push_back(control);
    info.push_back(sequence);
    info.push_back(variant);
    // length big-endian
    info.push_back((uint8_t)((orig_length >> 8) & 0xFF));
    info.push_back((uint8_t)(orig_length & 0xFF));
    // encoded payload
    info.insert(info.end(), encoded_payload.begin(), encoded_payload.end());
    // FCS big-endian
    info.push_back((uint8_t)((fcs >> 8) & 0xFF));
    info.push_back((uint8_t)(fcs & 0xFF));
    return info;
}

bool parse_information_field_with_fcs(const std::vector<uint8_t>& info, FrameInfo& frame) {
    // минимальная длина: address(1)+control(1)+seq(1)+variant(1)+length(2)+FCS(2) = 8
    if (info.size() < 8) return false;
    size_t idx = 0;
    frame.address = info[idx++];
    frame.control = info[idx++];
    frame.sequence = info[idx++];
    frame.variant = info[idx++];
    frame.length = (uint16_t(info[idx]) << 8) | uint16_t(info[idx + 1]);
    idx += 2;
    size_t encoded_len = size_t(frame.length) * 2; // каждый байт -> 2 закодированных байта
    if (idx + encoded_len + 2 > info.size()) {
        return false; // несоответствие размеров
    }
    std::vector<uint8_t> encoded_payload(info.begin() + idx, info.begin() + idx + encoded_len);
    idx += encoded_len;
    frame.fcs = (uint16_t(info[idx]) << 8) | uint16_t(info[idx + 1]);
    idx += 2;
    // декодируем закодированный payload
    DecodeResult dr = hamming_decode_payload(encoded_payload, frame.length);
    frame.payload = std::move(dr.decoded);
    frame.had_single_error = dr.had_single_error;
    frame.had_double_error = dr.had_double_error;
    frame.valid = true;
    return true;
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

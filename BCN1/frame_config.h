#pragma once
#include <vector>

#define FLAG 0x04

struct FrameInfo {
    uint8_t address = 0;
    uint8_t control = 0;
    uint8_t sequence = 0;
    uint8_t variant = 0;
    uint16_t length = 0; // длина оригинального payload (байты)
    std::vector<uint8_t> payload; // декодированный payload (байты)
    uint16_t fcs = 0; // CRC16 по оригинальному payload
    std::vector<uint8_t> raw_frame; // байты кадра после байт-стаффинга (включая флаги)
    bool valid = false;
    bool had_single_error = false;
    bool had_double_error = false;
};


std::vector<uint8_t> build_information_field_with_fcs_and_length(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& encoded_payload, uint16_t orig_length, uint16_t fcs);
std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data);
std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data);
bool parse_information_field_with_fcs(const std::vector<uint8_t>& info, FrameInfo& frame);
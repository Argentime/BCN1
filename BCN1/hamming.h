#pragma once
#include <vector>

enum class HammingStatus { OK = 0, CORRECTED = 1, DOUBLE_ERROR = 2 };

struct HammingResult {
    uint8_t nibble; // decoded 4 bits in low nibble
    HammingStatus status;
};

struct DecodeResult {
    std::vector<uint8_t> decoded;
    bool had_single_error;
    bool had_double_error;
};

uint8_t hamming_encode_nibble(uint8_t nibble4);
HammingResult hamming_decode_byte(uint8_t code);
std::vector<uint8_t> hamming_encode_payload(const std::vector<uint8_t>& payload);
DecodeResult hamming_decode_payload(const std::vector<uint8_t>& encoded, size_t orig_length);
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

struct DecodeHammingParityResult {
    std::vector<uint8_t> decoded_payload; // Декодированный payload (исправленный)
    bool had_single_error;
    bool had_double_error;
};

uint8_t hamming_encode_nibble(uint8_t nibble4); // Это останется для внутренней логики
HammingResult hamming_decode_byte(uint8_t code); // Это останется для внутренней логики

// Новая функция: генерирует только проверочные биты для всего payload
std::vector<uint8_t> hamming_generate_parity_bits(const std::vector<uint8_t>& data);

// Новая функция: декодирует payload, используя проверочные биты из FCS
DecodeHammingParityResult hamming_decode_with_parity_bits(
    const std::vector<uint8_t>& payload,
    const std::vector<uint8_t>& parity_bits
);
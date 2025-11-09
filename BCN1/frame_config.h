#pragma once
#include <vector>
#include <cstdint>
#include "hamming.h"

#define FLAG 0x04

struct FrameInfo {
    uint8_t address = 0;
    uint8_t control = 0;
    uint8_t sequence = 0;
    uint8_t variant = 0;
    // --- ПОЛЕ length УДАЛЕНО ---
    // uint16_t length = 0;

    std::vector<uint8_t> payload;
    std::vector<uint8_t> fcs_parity_bits;
    std::vector<uint8_t> raw_frame;

    bool valid = false;
    bool had_single_error = false;
    bool had_double_error = false;
};

// Прототип обновлен: убран параметр orig_length
std::vector<uint8_t> build_information_field_with_dynamic_fcs(
    uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& raw_payload,
    const std::vector<uint8_t>& hamming_parity_bits
);

// Прототип не меняется, но внутренняя логика изменится
bool parse_information_field_with_dynamic_fcs(std::vector<uint8_t>& info, FrameInfo& frame);

std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data);
std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data);
#pragma once
#include <vector>
#include <cstdint> // Для uint8_t
#include "hamming.h" // Для HammingStatus и DecodeHammingParityResult

#define FLAG 0x04

struct FrameInfo {
    uint8_t address = 0;
    uint8_t control = 0;
    uint8_t sequence = 0;
    uint8_t variant = 0;
    uint16_t length = 0; // длина оригинального payload (байты)
    std::vector<uint8_t> payload; // оригинальный payload
    std::vector<uint8_t> fcs_parity_bits; // FCS теперь хранит только проверочные биты Хэмминга
    std::vector<uint8_t> raw_frame; // байты кадра после байт-стаффинга (включая флаги)

    // Статус всего кадра после декодирования Хэмминга
    bool valid = false; // Нет ли двойных ошибок и совпадает ли payload
    bool had_single_error = false;
    bool had_double_error = false;
};

// Новая функция: строит информационное поле, где FCS - это проверочные биты Хэмминга
std::vector<uint8_t> build_information_field_with_dynamic_fcs(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& raw_payload, uint16_t orig_length, const std::vector<uint8_t>& hamming_parity_bits);

// Новая функция: парсит информационное поле с динамическим FCS
bool parse_information_field_with_dynamic_fcs(std::vector<uint8_t>& info, FrameInfo& frame);

std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data);
std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data);
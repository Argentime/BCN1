#include <wtypes.h>
#include <iostream>
#include <vector>
#include "frame_config.h"
#include "hamming.h" // Для HammingStatus, DecodeHammingParityResult, hamming_decode_with_parity_bits

std::vector<uint8_t> build_information_field_with_dynamic_fcs(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& raw_payload, uint16_t orig_length, const std::vector<uint8_t>& hamming_parity_bits) {

    std::vector<uint8_t> info;
    info.push_back(address);
    info.push_back(control);
    info.push_back(sequence);
    info.push_back(variant);
    // length big-endian
    info.push_back((uint8_t)((orig_length >> 8) & 0xFF));
    info.push_back((uint8_t)(orig_length & 0xFF));
    // original payload
    info.insert(info.end(), raw_payload.begin(), raw_payload.end());
    // FCS (Hamming parity bits)
    info.insert(info.end(), hamming_parity_bits.begin(), hamming_parity_bits.end());
    return info;
}

bool parse_information_field_with_dynamic_fcs(std::vector<uint8_t>& info, FrameInfo& frame) {
    // минимальная длина: address(1)+control(1)+seq(1)+variant(1)+length(2) = 6
    // FCS теперь динамического размера, поэтому минимум будет 6.
    if (info.size() < 6) return false;

    size_t idx = 0;
    frame.address = info[idx++];
    frame.control = info[idx++];
    frame.sequence = info[idx++];
    frame.variant = info[idx++];
    frame.length = (uint16_t(info[idx]) << 8) | uint16_t(info[idx + 1]);
    idx += 2;

    // Проверяем, достаточно ли байт для payload и хотя бы 1 байта FCS (если payload не 0)
    // Если payload пуст, FCS тоже будет пуст (так как нет нибблов для проверки)
    if (idx + frame.length > info.size()) {
        return false; // Недостаточно данных для payload
    }

    frame.payload.assign(info.begin() + idx, info.begin() + idx + frame.length);
    idx += frame.length;

    // Оставшиеся байты - это FCS (проверочные биты Хэмминга)
    frame.fcs_parity_bits.assign(info.begin() + idx, info.end());

    // --- Декодирование Хэмминга с динамическим FCS ---
    // Здесь мы должны имитировать получение искаженного payload и parity_bits.
    // На самом деле, искажение происходит ДО ЭТОГО.
    // Для простоты, `frame.payload` уже содержит "полученные" данные,
    // и `frame.fcs_parity_bits` уже содержит "полученные" проверочные биты.
    // Теперь применяем к ним логику исправления.

    DecodeHammingParityResult hamming_result = hamming_decode_with_parity_bits(frame.payload, frame.fcs_parity_bits);

    frame.payload = hamming_result.decoded_payload; // Обновляем payload исправленными данными
    frame.had_single_error = hamming_result.had_single_error;
    frame.had_double_error = hamming_result.had_double_error;

    // Кадр считается валидным, если не было двойных ошибок
    frame.valid = !frame.had_double_error;

    return true;
}

// Byte stuffing/unstuffing (без изменений)
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
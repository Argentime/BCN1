#include <wtypes.h>
#include <iostream>
#include <vector>
#include "frame_config.h"
#include "hamming.h" // Для HammingStatus, DecodeHammingParityResult, hamming_decode_with_parity_bits

std::vector<uint8_t> build_information_field_with_dynamic_fcs(
    uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant,
    const std::vector<uint8_t>& raw_payload,
    const std::vector<uint8_t>& hamming_parity_bits) {

    std::vector<uint8_t> info;
    info.push_back(address);
    info.push_back(control);
    info.push_back(sequence);
    info.push_back(variant);

    // --- БЛОК ЗАПИСИ ДЛИНЫ УДАЛЁН ---

    // Сразу добавляем payload
    info.insert(info.end(), raw_payload.begin(), raw_payload.end());
    // И FCS
    info.insert(info.end(), hamming_parity_bits.begin(), hamming_parity_bits.end());
    return info;
}

// Функция разбора кадра: теперь вычисляет длину самостоятельно
bool parse_information_field_with_dynamic_fcs(std::vector<uint8_t>& info, FrameInfo& frame) {
    // Минимальная длина: 4 байта заголовка. Payload и FCS могут быть пустыми.
    if (info.size() < 4) return false;

    size_t idx = 0;
    frame.address = info[idx++];
    frame.control = info[idx++];
    frame.sequence = info[idx++];
    frame.variant = info[idx++];

    // --- НОВАЯ ЛОГИКА ОПРЕДЕЛЕНИЯ ДЛИНЫ ---
    // Вычисляем общий размер блока данных (payload + FCS)
    size_t data_and_fcs_size = info.size() - idx;

    // Если размер нечётный, кадр повреждён, так как payload и FCS должны быть равны
    if (data_and_fcs_size % 2 != 0) {
        return false; // Ошибка: нечётное количество байт
    }

    size_t payload_size = data_and_fcs_size / 2;
    // Поле length в структуре больше не существует, но мы используем локальную переменную

    frame.payload.assign(info.begin() + idx, info.begin() + idx + payload_size);
    idx += payload_size;

    frame.fcs_parity_bits.assign(info.begin() + idx, info.end());

    // Дальнейшая логика декодирования Хэмминга остаётся без изменений
    DecodeHammingParityResult hamming_result = hamming_decode_with_parity_bits(frame.payload, frame.fcs_parity_bits);

    frame.payload = hamming_result.decoded_payload;
    frame.had_single_error = hamming_result.had_single_error;
    frame.had_double_error = hamming_result.had_double_error;
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
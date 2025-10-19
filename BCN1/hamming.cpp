#include <vector>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <utility>
#include "io.h"
#include "hamming.h"

uint8_t hamming_encode_nibble(uint8_t nibble4) {
    // nibble4: bits b0..b3 (LSB..MSB) => d1..d4
    int d1 = (nibble4 >> 0) & 1;
    int d2 = (nibble4 >> 1) & 1;
    int d3 = (nibble4 >> 2) & 1;
    int d4 = (nibble4 >> 3) & 1;

    int p1 = d1 ^ d2 ^ d4;       // parity for positions 1 (covers 1,3,5,7 -> bits 3,5,7 correspond to d1,d2,d4)
    int p2 = d1 ^ d3 ^ d4;       // covers positions 2,3,6,7 -> d1,d3,d4
    int p3 = d2 ^ d3 ^ d4;       // covers positions 4,5,6,7 -> d2,d3,d4

    // compose bits into byte (positions -> bit indices)
    uint8_t code = 0;
    // set positions 3,5,6,7 data
    if (d1) code |= (1 << (3 - 1));
    if (d2) code |= (1 << (5 - 1));
    if (d3) code |= (1 << (6 - 1));
    if (d4) code |= (1 << (7 - 1));
    if (p1) code |= (1 << (1 - 1));
    if (p2) code |= (1 << (2 - 1));
    if (p3) code |= (1 << (4 - 1));
    // overall parity p0 such that parity of bits1..8 == 0 (even)
    int parity = 0;
    for (int pos = 1; pos <= 7; ++pos) {
        parity ^= ((code >> (pos - 1)) & 1);
    }
    int p0 = parity; // set so total parity even
    if (p0) code |= (1 << (8 - 1));
    return code;
}

// decode one 8-bit code -> nibble + status

HammingResult hamming_decode_byte(uint8_t code) {
    // extract bits positions 1..8
    auto bit = [&](int pos)->int { return (code >> (pos - 1)) & 1; };

    // recompute parity checks (p1', p2', p3')
    int p1_check = bit(1) ^ bit(3) ^ bit(5) ^ bit(7); // should be 0 for no error
    int p2_check = bit(2) ^ bit(3) ^ bit(6) ^ bit(7);
    int p3_check = bit(4) ^ bit(5) ^ bit(6) ^ bit(7);
    int syndrome = (p3_check << 2) | (p2_check << 1) | (p1_check << 0); // binary index (3..1)

    // overall parity check
    int parity_all = 0;
    for (int pos = 1; pos <= 8; ++pos) parity_all ^= bit(pos);

    if (syndrome == 0 && parity_all == 0) {
        // no error
        uint8_t d1 = bit(3), d2 = bit(5), d3 = bit(6), d4 = bit(7);
        uint8_t nib = (d1 << 0) | (d2 << 1) | (d3 << 2) | (d4 << 3);
        return { nib, HammingStatus::OK };
    }
    if (syndrome == 0 && parity_all == 1) {
        // single-bit error in overall parity bit (pos 8)
        // correct by flipping pos8
        code ^= (1 << (8 - 1));
        uint8_t d1 = (code >> (3 - 1)) & 1;
        uint8_t d2 = (code >> (5 - 1)) & 1;
        uint8_t d3 = (code >> (6 - 1)) & 1;
        uint8_t d4 = (code >> (7 - 1)) & 1;
        uint8_t nib = (d1 << 0) | (d2 << 1) | (d3 << 2) | (d4 << 3);
        return { nib, HammingStatus::CORRECTED };
    }
    if (syndrome != 0 && parity_all == 1) {
        // single-bit error at position = syndrome (1..7)
        int pos = syndrome;
        code ^= (1 << (pos - 1)); // flip erroneous bit
        uint8_t d1 = (code >> (3 - 1)) & 1;
        uint8_t d2 = (code >> (5 - 1)) & 1;
        uint8_t d3 = (code >> (6 - 1)) & 1;
        uint8_t d4 = (code >> (7 - 1)) & 1;
        uint8_t nib = (d1 << 0) | (d2 << 1) | (d3 << 2) | (d4 << 3);
        return { nib, HammingStatus::CORRECTED };
    }
    if (syndrome != 0 && parity_all == 0) {
        // syndrome nonzero but overall parity even => detected double-bit error (uncorrectable)
        return { 0, HammingStatus::DOUBLE_ERROR };
    }
    // Fallback
    return { 0, HammingStatus::DOUBLE_ERROR };
}

// encode whole payload: each input byte -> two code bytes
std::vector<uint8_t> hamming_encode_payload(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> encoded;
    encoded.reserve(payload.size() * 2);
    for (uint8_t b : payload) {
        uint8_t low = b & 0x0F;
        uint8_t high = (b >> 4) & 0x0F;
        encoded.push_back(hamming_encode_nibble(low));
        encoded.push_back(hamming_encode_nibble(high));
    }
    return encoded;
}

// decode whole encoded payload (encoded.size() must be == original_length*2)
// returns tuple(decoded_payload, had_single_error, had_double_error)

DecodeResult hamming_decode_payload(const std::vector<uint8_t>& encoded, size_t orig_length) {
    DecodeResult res;
    res.decoded.resize(orig_length);
    res.had_single_error = false;
    res.had_double_error = false;
    // encoded length expected = orig_length*2
    if (encoded.size() < orig_length * 2) {
        // malformed
        res.decoded.clear();
        res.had_double_error = true;
        return res;
    }
    for (size_t i = 0; i < orig_length; ++i) {
        uint8_t code_low = encoded[2 * i];
        uint8_t code_high = encoded[2 * i + 1];
        HammingResult r1 = hamming_decode_byte(code_low);
        HammingResult r2 = hamming_decode_byte(code_high);
        if (r1.status == HammingStatus::DOUBLE_ERROR || r2.status == HammingStatus::DOUBLE_ERROR) {
            res.had_double_error = true;
        }
        if (r1.status == HammingStatus::CORRECTED || r2.status == HammingStatus::CORRECTED) {
            res.had_single_error = true;
        }
        uint8_t byte = ((r2.nibble & 0x0F) << 4) | (r1.nibble & 0x0F);
        res.decoded[i] = byte;
    }
    return res;
}

std::vector<uint8_t> hamming_generate_parity_bits(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> all_parity_bits;
    all_parity_bits.reserve(data.size()); // Ожидаем примерно 1 байт проверочных бит на 1 байт данных

    uint8_t current_parity_byte = 0;
    int bits_in_current_parity_byte = 0;

    for (uint8_t b : data) {
        // Для младшего ниббла
        uint8_t nibble_low = b & 0x0F;
        int d1 = (nibble_low >> 0) & 1;
        int d2 = (nibble_low >> 1) & 1;
        int d3 = (nibble_low >> 2) & 1;
        int d4 = (nibble_low >> 3) & 1;

        int p1_low = d1 ^ d2 ^ d4;
        int p2_low = d1 ^ d3 ^ d4;
        int p3_low = d2 ^ d3 ^ d4;

        uint8_t code_low_7_bits = 0;
        if (d1) code_low_7_bits |= (1 << (3 - 1)); // pos 3
        if (d2) code_low_7_bits |= (1 << (5 - 1)); // pos 5
        if (d3) code_low_7_bits |= (1 << (6 - 1)); // pos 6
        if (d4) code_low_7_bits |= (1 << (7 - 1)); // pos 7
        if (p1_low) code_low_7_bits |= (1 << (1 - 1)); // pos 1
        if (p2_low) code_low_7_bits |= (1 << (2 - 1)); // pos 2
        if (p3_low) code_low_7_bits |= (1 << (4 - 1)); // pos 4

        int parity_low_all = 0;
        for (int pos = 1; pos <= 7; ++pos) {
            parity_low_all ^= ((code_low_7_bits >> (pos - 1)) & 1);
        }
        int p0_low = parity_low_all; // Общая четность

        // Добавляем 4 проверочных бита (p0, p1, p2, p3)
        current_parity_byte |= (p0_low << (bits_in_current_parity_byte++));
        current_parity_byte |= (p1_low << (bits_in_current_parity_byte++));
        current_parity_byte |= (p2_low << (bits_in_current_parity_byte++));
        current_parity_byte |= (p3_low << (bits_in_current_parity_byte++));
        if (bits_in_current_parity_byte == 8) {
            all_parity_bits.push_back(current_parity_byte);
            current_parity_byte = 0;
            bits_in_current_parity_byte = 0;
        }

        // Для старшего ниббла
        uint8_t nibble_high = (b >> 4) & 0x0F;
        d1 = (nibble_high >> 0) & 1;
        d2 = (nibble_high >> 1) & 1;
        d3 = (nibble_high >> 2) & 1;
        d4 = (nibble_high >> 3) & 1;

        int p1_high = d1 ^ d2 ^ d4;
        int p2_high = d1 ^ d3 ^ d4;
        int p3_high = d2 ^ d3 ^ d4;

        uint8_t code_high_7_bits = 0;
        if (d1) code_high_7_bits |= (1 << (3 - 1)); // pos 3
        if (d2) code_high_7_bits |= (1 << (5 - 1)); // pos 5
        if (d3) code_high_7_bits |= (1 << (6 - 1)); // pos 6
        if (d4) code_high_7_bits |= (1 << (7 - 1)); // pos 7
        if (p1_high) code_high_7_bits |= (1 << (1 - 1)); // pos 1
        if (p2_high) code_high_7_bits |= (1 << (2 - 1)); // pos 2
        if (p3_high) code_high_7_bits |= (1 << (4 - 1)); // pos 4

        int parity_high_all = 0;
        for (int pos = 1; pos <= 7; ++pos) {
            parity_high_all ^= ((code_high_7_bits >> (pos - 1)) & 1);
        }
        int p0_high = parity_high_all; // Общая четность

        // Добавляем 4 проверочных бита (p0, p1, p2, p3)
        current_parity_byte |= (p0_high << (bits_in_current_parity_byte++));
        current_parity_byte |= (p1_high << (bits_in_current_parity_byte++));
        current_parity_byte |= (p2_high << (bits_in_current_parity_byte++));
        current_parity_byte |= (p3_high << (bits_in_current_parity_byte++));
        if (bits_in_current_parity_byte == 8) {
            all_parity_bits.push_back(current_parity_byte);
            current_parity_byte = 0;
            bits_in_current_parity_byte = 0;
        }
    }

    if (bits_in_current_parity_byte > 0) { // Добавляем оставшиеся биты, если есть
        all_parity_bits.push_back(current_parity_byte);
    }

    return all_parity_bits;
}


// Новая функция: декодирует payload, используя проверочные биты из FCS
DecodeHammingParityResult hamming_decode_with_parity_bits(
    const std::vector<uint8_t>& payload,
    const std::vector<uint8_t>& parity_bits_vector
) {
    DecodeHammingParityResult result;
    result.decoded_payload = payload; // Начинаем с копии полученного payload
    result.had_single_error = false;
    result.had_double_error = false;

    // Проходим по каждому байту payload, затем по каждому нибблу
    // Извлекаем соответствующие 4 проверочных бита из parity_bits_vector
    int parity_bit_idx = 0; // Индекс текущего проверочного бита в parity_bits_vector

    for (size_t byte_idx = 0; byte_idx < payload.size(); ++byte_idx) {
        uint8_t current_byte = payload[byte_idx];

        // Декодируем младший ниббл
        uint8_t nibble_low = current_byte & 0x0F;
        int d1_orig_low = (nibble_low >> 0) & 1;
        int d2_orig_low = (nibble_low >> 1) & 1;
        int d3_orig_low = (nibble_low >> 2) & 1;
        int d4_orig_low = (nibble_low >> 3) & 1;

        if (parity_bit_idx + 3 < parity_bits_vector.size() * 8) { // Убедимся, что есть достаточно проверочных бит
            int p0_received_low = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p1_received_low = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p2_received_low = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p3_received_low = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;

            // Собираем 8-битное слово для декодирования
            uint8_t code_to_decode_low = 0;
            if (d1_orig_low) code_to_decode_low |= (1 << (3 - 1));
            if (d2_orig_low) code_to_decode_low |= (1 << (5 - 1));
            if (d3_orig_low) code_to_decode_low |= (1 << (6 - 1));
            if (d4_orig_low) code_to_decode_low |= (1 << (7 - 1));
            if (p1_received_low) code_to_decode_low |= (1 << (1 - 1));
            if (p2_received_low) code_to_decode_low |= (1 << (2 - 1));
            if (p3_received_low) code_to_decode_low |= (1 << (4 - 1));
            if (p0_received_low) code_to_decode_low |= (1 << (8 - 1));

            HammingResult hr_low = hamming_decode_byte(code_to_decode_low);

            if (hr_low.status == HammingStatus::DOUBLE_ERROR) {
                result.had_double_error = true;
                // В случае двойной ошибки мы не можем исправить, оставляем как есть или помечаем
                // Для этого варианта просто помечаем и продолжаем
            }
            else if (hr_low.status == HammingStatus::CORRECTED) {
                result.had_single_error = true;
                // Обновляем ниббл в payload
                result.decoded_payload[byte_idx] = (result.decoded_payload[byte_idx] & 0xF0) | hr_low.nibble;
            }
        }
        else {
            // Недостаточно проверочных бит, ошибка в кадре
            result.had_double_error = true;
        }

        // Декодируем старший ниббл
        uint8_t nibble_high = (current_byte >> 4) & 0x0F;
        int d1_orig_high = (nibble_high >> 0) & 1;
        int d2_orig_high = (nibble_high >> 1) & 1;
        int d3_orig_high = (nibble_high >> 2) & 1;
        int d4_orig_high = (nibble_high >> 3) & 1;

        if (parity_bit_idx + 3 < parity_bits_vector.size() * 8) { // Убедимся, что есть достаточно проверочных бит
            int p0_received_high = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p1_received_high = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p2_received_high = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;
            int p3_received_high = (parity_bits_vector[parity_bit_idx / 8] >> (parity_bit_idx % 8)) & 1; parity_bit_idx++;

            // Собираем 8-битное слово для декодирования
            uint8_t code_to_decode_high = 0;
            if (d1_orig_high) code_to_decode_high |= (1 << (3 - 1));
            if (d2_orig_high) code_to_decode_high |= (1 << (5 - 1));
            if (d3_orig_high) code_to_decode_high |= (1 << (6 - 1));
            if (d4_orig_high) code_to_decode_high |= (1 << (7 - 1));
            if (p1_received_high) code_to_decode_high |= (1 << (1 - 1));
            if (p2_received_high) code_to_decode_high |= (1 << (2 - 1));
            if (p3_received_high) code_to_decode_high |= (1 << (4 - 1));
            if (p0_received_high) code_to_decode_high |= (1 << (8 - 1));

            HammingResult hr_high = hamming_decode_byte(code_to_decode_high);

            if (hr_high.status == HammingStatus::DOUBLE_ERROR) {
                result.had_double_error = true;
            }
            else if (hr_high.status == HammingStatus::CORRECTED) {
                result.had_single_error = true;
                // Обновляем ниббл в payload
                result.decoded_payload[byte_idx] = (result.decoded_payload[byte_idx] & 0x0F) | (hr_high.nibble << 4);
            }
        }
        else {
            result.had_double_error = true;
        }
    }
    return result;
}
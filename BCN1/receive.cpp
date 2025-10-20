#include <vector>
#include <iostream>
#include <random>
#include "frame_config.h"
#include "hamming.h"
#include "com_config.h"
#include "io.h"
#include "cli.h"

void distort_frame_data(std::vector<uint8_t>& payload, std::mt19937& rng) {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    double p = prob(rng);
    int bits_to_flip = 0;
    if (p < 0.75) bits_to_flip = 1; // 75% -> 1 бит
    else bits_to_flip = 2; // 25% -> 2 бита (остается 5% без ошибок)

    if (bits_to_flip == 0) return;

    for (int k = 0; k < bits_to_flip; ++k) {
        // Случайным образом выбираем, исказить ли payload или parity_bits

        if (!payload.empty()) {
            std::uniform_int_distribution<size_t> byte_idx_dist(0, payload.size() - 1);
            std::uniform_int_distribution<int> bit_dist(0, 7);
            size_t byte_idx = byte_idx_dist(rng);
            int bit = bit_dist(rng);
            payload[byte_idx] ^= (1 << bit);
        }
    }
}


bool receive_frame(HANDLE hComm, std::mt19937& rng) {
    std::vector<uint8_t> full_message; // объединённый декодированный payload
    char byte;
    DWORD bytesRead;
    std::vector<uint8_t> buffer;
    bool started = false;
    bool any_frame = false;

    while (true) {
        COMSTAT comStat;
        DWORD dwError;
        ClearCommError(hComm, &dwError, &comStat);

        if (comStat.cbInQue == 0) break;

        if (!ReadFile(hComm, &byte, 1, &bytesRead, NULL) || bytesRead == 0)
            continue;

        uint8_t b = static_cast<uint8_t>(byte);

        if (b == FLAG) {
            if (!started) {
                started = true;
                buffer.clear();
                buffer.push_back(b);
            }
            else {
                buffer.push_back(b);
                // получили полный кадр в buffer
                std::vector<uint8_t> unstuffed = byte_unstuff(buffer); // без флагов
                FrameInfo frameTemp;

                if (unstuffed.size() >= 6) { // Min size: address(1)+control(1)+seq(1)+variant(1)+length(2)
                    size_t idx = 0;
                    uint8_t address = unstuffed[idx++];
                    uint8_t control = unstuffed[idx++];
                    uint8_t sequence = unstuffed[idx++];
                    uint8_t variant = unstuffed[idx++];
                    uint16_t length = (uint16_t(unstuffed[idx]) << 8) | uint16_t(unstuffed[idx + 1]);
                    idx += 2;

                    // Убеждаемся, что есть достаточно данных для payload
                    if (idx + length <= unstuffed.size()) {
                        std::vector<uint8_t> received_payload(unstuffed.begin() + idx, unstuffed.begin() + idx + length);
                        idx += length;
                        std::vector<uint8_t> received_parity_bits(unstuffed.begin() + idx, unstuffed.end());

                        // --- Искажение данных ---
                        distort_frame_data(received_payload, rng);
                        // --- Конец искажения ---

                        // Применяем декодирование Хэмминга к (возможно) искаженным данным
                        DecodeHammingParityResult hamming_res = hamming_decode_with_parity_bits(received_payload, received_parity_bits);

                        frameTemp.address = address;
                        frameTemp.control = control;
                        frameTemp.sequence = sequence;
                        frameTemp.variant = variant;
                        frameTemp.length = length;
                        frameTemp.payload = hamming_res.decoded_payload; // Уже исправленный payload
                        frameTemp.fcs_parity_bits = received_parity_bits; // Сохраняем полученные (возможно искаженные) parity bits
                        frameTemp.had_single_error = hamming_res.had_single_error;
                        frameTemp.had_double_error = hamming_res.had_double_error;
                        frameTemp.valid = !frameTemp.had_double_error; // Кадр валиден, если нет двойных ошибок

                        frameTemp.raw_frame = buffer; // Сохраняем весь raw кадр для печати

                        full_message.insert(full_message.end(), frameTemp.payload.begin(), frameTemp.payload.end());
                        any_frame = true;
                        if (frameTemp.had_double_error) {
                            std::cout << "Обнаружена двойная ошибка, данные повреждены." << std::endl;
                        }

                    }
                }
                started = false;
                buffer.clear();
            }
        }
        else if (started) {
            buffer.push_back(b);
        }
    }

    if (!any_frame) {
        std::cout << "Нет данных для чтения." << std::endl;
    }
    else {
        std::cout << "Принято сообщение:" << std::endl;
        std::string message(full_message.begin(), full_message.end());
        std::cout << message << std::endl;
    }

    return any_frame;
}
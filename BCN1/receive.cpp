#include <vector>
#include <iostream>
#include <random>
#include "frame_config.h"
#include "hamming.h"
#include "com_config.h"
#include "io.h"

void distort_encoded_payload(std::vector<uint8_t>& encoded_payload, std::mt19937& rng) {
    if (encoded_payload.empty()) return;
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    double p = prob(rng);
    int bits_to_flip = (p < 0.75) ? 1 : 2; // 75% -> 1, 25% -> 2

    std::uniform_int_distribution<size_t> idx_dist(0, encoded_payload.size() - 1);
    std::uniform_int_distribution<int> bit_dist(0, 7);

    // Если нужно два бита — выбираем, возможно, два разных байта (или один и тот же)
    for (int k = 0; k < bits_to_flip; ++k) {
        size_t byte_idx = idx_dist(rng);
        int bit = bit_dist(rng);
        encoded_payload[byte_idx] ^= (1 << bit);
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
                if (unstuffed.size() >= 6) { // минимум для length
                    uint16_t length = (uint16_t(unstuffed[4]) << 8) | uint16_t(unstuffed[5]);
                    size_t encoded_len = size_t(length) * 2;
                    size_t expected_total = 1 + 1 + 1 + 1 + 2 + encoded_len + 2; // address..variant(4)+len(2)+encoded+FCS(2)
                    if (unstuffed.size() >= expected_total) {
                        size_t idx = 0;
                        frameTemp.address = unstuffed[idx++];
                        frameTemp.control = unstuffed[idx++];
                        frameTemp.sequence = unstuffed[idx++];
                        frameTemp.variant = unstuffed[idx++];
                        frameTemp.length = (uint16_t(unstuffed[idx]) << 8) | uint16_t(unstuffed[idx + 1]);
                        idx += 2;
                        std::vector<uint8_t> encoded_payload(unstuffed.begin() + idx, unstuffed.begin() + idx + encoded_len);
                        idx += encoded_len;
                        uint16_t fcs = (uint16_t(unstuffed[idx]) << 8) | uint16_t(unstuffed[idx + 1]);
                        idx += 2;

                        // Искажение битов
                        distort_encoded_payload(encoded_payload, rng);

                        // Декодирование Хэмминга
                        DecodeResult dr = hamming_decode_payload(encoded_payload, frameTemp.length);
                        frameTemp.payload = std::move(dr.decoded);
                        frameTemp.had_single_error = dr.had_single_error;
                        frameTemp.had_double_error = dr.had_double_error;
                        frameTemp.fcs = fcs;

                        // Проверка FCS
                        uint16_t calc_fcs = crc16_ccitt(frameTemp.payload);
                        frameTemp.valid = (calc_fcs == frameTemp.fcs);

                        // raw_frame
                        frameTemp.raw_frame = buffer;

                        full_message.insert(full_message.end(), frameTemp.payload.begin(), frameTemp.payload.end());
                        any_frame = true;
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

    if (any_frame) {
        std::string message(full_message.begin(), full_message.end());
        std::cout << message << std::endl;
    }
    else {
        std::cout << "Нет данных для чтения." << std::endl;
    }

    return any_frame;
}

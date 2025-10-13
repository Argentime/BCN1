#include <vector>
#include "hamming.h"
#include <random>
#include <wtypes.h>
#include "com_config.h"
#include "frame_config.h"


bool split_info_into_payload_and_fcs(const std::vector<uint8_t>& info, std::vector<uint8_t>& payload_out, std::vector<uint8_t>& fcs_out) {
    // info = [addr(1), ctrl(1), seq(1), variant(1), payload (P bytes), fcs (F bytes)]
    if (info.size() < 4) return false;
    size_t L = info.size();
    size_t header = 4;
    // try all possible payload lengths P from 0..L-header
    for (size_t P = 0; P <= L - header; ++P) {
        size_t k_bits = P * 8;
        int m = compute_hamming_m(k_bits);
        size_t fcs_bits = (size_t)m + 1;
        size_t fcs_bytes_needed = (fcs_bits + 7) / 8;
        if (header + P + fcs_bytes_needed == L) {
            // match found
            payload_out.assign(info.begin() + header, info.begin() + header + P);
            fcs_out.assign(info.begin() + header + P, info.end());
            return true;
        }
    }
    // if none matched - treat as no FCS (fallback: all is payload)
    payload_out.assign(info.begin() + header, info.end());
    fcs_out.clear();
    return true;
}

// function to randomly corrupt 1 or 2 bits in payload with probabilities 75% (1 bit) and 25% (2 bits)
void randomly_corrupt_payload_bits(std::vector<uint8_t>& payload) {
    if (payload.empty()) return;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> prob(1, 100);
    int p = prob(gen);
    int flips = (p <= 75) ? 1 : 2;
    std::uniform_int_distribution<size_t> bitpos(0, payload.size() * 8 - 1);
    for (int f = 0; f < flips; ++f) {
        size_t bit = bitpos(gen);
        size_t byte_idx = bit / 8;
        size_t bit_in_byte = bit % 8;
        payload[byte_idx] ^= (uint8_t)(1u << bit_in_byte);
    }
}

// получение кадра: читаем все байты из порта, собираем кадры и объединяем payloads
bool receive_frame(HANDLE hComm) {
    std::vector<uint8_t> full_message; // для вывода всего сообщения
    std::vector<uint8_t> raw_buffer;
    // Читаем все доступные байты
    if (!read_from_port(hComm, raw_buffer)) {
        std::cout << "Нет данных для чтения." << std::endl;
        return false;
    }
    // В raw_buffer может быть множество байтов, возможно несколько кадров подряд.
    // Соберём кадры: ищем FLAG..FLAG
    size_t idx = 0;
    FrameInfo last_chunk_frame_local;
    bool any_frame = false;
    while (idx < raw_buffer.size()) {
        // найти старт
        while (idx < raw_buffer.size() && raw_buffer[idx] != FLAG) ++idx;
        if (idx >= raw_buffer.size()) break;
        size_t start = idx;
        ++idx;
        // найти конец
        while (idx < raw_buffer.size() && raw_buffer[idx] != FLAG) ++idx;
        if (idx >= raw_buffer.size()) break;
        size_t end = idx; // position of FLAG
        // extract chunk [start..end]
        std::vector<uint8_t> chunk(raw_buffer.begin() + start, raw_buffer.begin() + end + 1);
        // de-stuff
        std::vector<uint8_t> info = byte_unstuff(chunk); // info = addr..variant..payload..fcs
        if (info.size() < 4) { idx = end + 1; continue; }
        // split into payload and fcs
        std::vector<uint8_t> payload, fcs;
        bool ok = split_info_into_payload_and_fcs(info, payload, fcs);
        if (!ok) { idx = end + 1; continue; }

        // сохранить заголовок fields for last_chunk_frame_local
        FrameInfo frame;
        parse_information_field(info, frame);
        frame.payload = payload;
        frame.raw_frame = chunk;
        frame.fcs_bytes = fcs;

        // Случайная порча битов в поле данных (после приема), как требует задание
        randomly_corrupt_payload_bits(frame.payload);

        // Проверка Hamming и коррекция (если возможно)
        if (!fcs.empty()) {
            auto corrected = hamming_check_and_correct(frame.payload, frame.fcs_bytes);
            int status = corrected.second;
            if (status == 0) {
                // no error
                // frame.payload remains
            }
            else if (status == 1) {
                // corrected single-bit error (or parity-bit error fixed)
                frame.payload = corrected.first;
                // можно логировать
                std::cout << "Одиночная ошибка обнаружена и исправлена в кадре (seq=" << (int)frame.sequence << ").\n";
            }
            else if (status == 2) {
                std::cout << "Двойная ошибка обнаружена (некорректируема) в кадре (seq=" << (int)frame.sequence << ").\n";
            }
            else if (status == 3) {
                std::cout << "Ошибка только в общем бите паритета (поправлен общий паритет).\n";
            }
        }
        else {
            // нет FCS — ничего не делаем
        }

        // Собираем payload для вывода всей строки
        full_message.insert(full_message.end(), frame.payload.begin(), frame.payload.end());

        any_frame = true;

        idx = end + 1;
    }

    if (any_frame) {
        if (!full_message.empty()) {
            std::string message(full_message.begin(), full_message.end());
            std::cout << "Принято сообщение: " << message << std::endl;
        }
        else {
            std::cout << "Принято сообщение (пустое).\n";
        }
        return true;
    }
    else {
        std::cout << "Нет полных кадров в буфере.\n";
        return false;
    }
}
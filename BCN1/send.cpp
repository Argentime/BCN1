#define NOMINMAX
#include <vector>
#include <string>
#include <algorithm>
#include "frame_config.h"
#include "com_config.h"
#include "hamming.h"
#include "io.h"

bool send_string_as_frame(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message, uint8_t& sequence,
    uint8_t address = 0x01, uint8_t control = 0x00, uint8_t variant = 0x00) {
    const size_t MAX_PAYLOAD_SIZE = 64; // оригинальный размер payload в байтах
    size_t offset = 0;
    bool any_sent = false;

    while (offset < message.size()) {
        size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size() - offset);
        std::vector<uint8_t> payload_chunk(message.begin() + offset, message.begin() + offset + chunk_size);

        // 1) Генерируем проверочные биты Хэмминга для payload_chunk
        std::vector<uint8_t> hamming_parity_bits = hamming_generate_parity_bits(payload_chunk);

        // 2) Формируем информационное поле: address|control|seq|variant|length(2)|payload|FCS(Hamming_parity_bits)
        std::vector<uint8_t> info = build_information_field_with_dynamic_fcs(address, control, sequence, variant,
            payload_chunk, (uint16_t)chunk_size, hamming_parity_bits);

        // 3) Байт-стаффинг
        std::vector<uint8_t> stuffed = byte_stuff(info);

        // Сохраняем последний отправленный кадр (логические поля + raw)
        // Для этого нужно временно скопировать unstuffed-версию `info` и распарсить ее
        std::vector<uint8_t> unstuffed_info_for_last_frame = info; // info уже unstuffed
        parse_information_field_with_dynamic_fcs(unstuffed_info_for_last_frame, last_sent_frame);
        last_sent_frame.raw_frame = stuffed; // Сохраняем raw_frame как стаффированный кадр

        // 4) Отправляем
        if (!write_to_port(hComm, stuffed)) {
            std::cout << "Ошибка отправки кадра.\n";
            return false;
        }

        ++sequence;
        offset += chunk_size;
        any_sent = true;
    }

    return any_sent;
}
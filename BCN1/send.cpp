#define NOMINMAX
#include <vector>
#include <string>
#include <algorithm>
#include "frame_config.h"
#include "com_config.h"
#include "hamming.h"
#include "io.h"

bool send_string_as_frame(FrameInfo &last_sent_frame, HANDLE hComm, const std::string& message, uint8_t& sequence,
    uint8_t address = 0x01, uint8_t control = 0x00, uint8_t variant = 0x00) {
    const size_t MAX_PAYLOAD_SIZE = 64; // оригинальный размер payload в байтах
    size_t offset = 0;
    bool any_sent = false;

    while (offset < message.size()) {
        size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size() - offset);
        std::vector<uint8_t> payload(message.begin() + offset, message.begin() + offset + chunk_size);

        // 1) вычисляем FCS по оригинальному payload
        uint16_t fcs = crc16_ccitt(payload);

        // 2) кодируем payload Хэммингом (каждый байт -> 2 байта)
        std::vector<uint8_t> encoded_payload = hamming_encode_payload(payload);

        // 3) формируем информационное поле: address|control|seq|variant|length(2)|encoded_payload|FCS(2)
        std::vector<uint8_t> info = build_information_field_with_fcs_and_length(address, control, sequence, variant, encoded_payload, (uint16_t)chunk_size, fcs);

        // 4) байт-стаффинг
        std::vector<uint8_t> stuffed = byte_stuff(info);

        // сохраняем последний отправленный кадр (логические поля + raw)
        parse_information_field_with_fcs(info, last_sent_frame); // info без флагов
        last_sent_frame.raw_frame = stuffed;

        // 5) отправляем
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
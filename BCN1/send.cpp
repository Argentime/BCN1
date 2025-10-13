#define NOMINMAX
#include <cstdint>
#include <wtypes.h>
#include <string>
#include <vector>
#include <algorithm>
#include "hamming.h"
#include "frame_config.h"
#include "com_config.h"
#include "send.h"
#include "io.h"


// функция для определения длины FCS (в байтах) для payload длины P байт
size_t compute_fcs_bytes_for_payload_bytes(size_t payload_bytes) {
    size_t k_bits = payload_bytes * 8;
    int m = compute_hamming_m(k_bits);
    int total_bits = m + 1; // includes overall parity
    return (total_bits + 7) / 8;
}

bool send_string_as_frame(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message, uint8_t& sequence,
    uint8_t address = 0x01, uint8_t control = 0x00, uint8_t variant = 0x00) {

    const size_t MAX_PAYLOAD_SIZE = 64; // bytes
    size_t offset = 0;
    while (offset < message.size()) {
        size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size() - offset);
        std::vector<uint8_t> payload(message.begin() + offset, message.begin() + offset + chunk_size);

        // build info (address..variant..payload)
        std::vector<uint8_t> info = build_information_field(address, control, sequence, variant, payload);

        // compute FCS bytes for this payload and append (Hamming SECDED)
        std::vector<uint8_t> fcs = build_fcs_from_payload(payload);
        // debug: print payload bits and fcs
        std::cout << "[SEND] payload bytes: ";
        for (auto b : payload) std::cout << to_hex_string(b) << " ";
        std::cout << "\n[SEND] payload bits (LSB-first per byte): ";
        for (auto b : payload) {
            for (int i = 0; i < 8; ++i) std::cout << ((b >> i) & 1);
            std::cout << " ";
        }
        std::cout << "\n[SEND] fcs bytes: ";
        for (auto b : fcs) std::cout << to_hex_string(b) << " ";
        std::cout << std::endl;

        // append raw fcs bytes to info
        info.insert(info.end(), fcs.begin(), fcs.end());

        // byte-stuff entire info+fcs and add flags
        std::vector<uint8_t> stuffed = byte_stuff(info);
        FrameInfo tmp;
        parse_information_field(info, tmp);
        tmp.payload = payload;
        tmp.raw_frame = stuffed;
        tmp.fcs_bytes = fcs;
        last_sent_frame = tmp;

        // save last_sent_frame as the last sent chunk (so menu shows last chunk)

        if (!write_to_port(hComm, stuffed)) {
            std::cout << "Ошибка отправки кадра.\n";
            return false;
        }

        ++sequence;
        offset += chunk_size;
    }

    std::cout << "Отправлено сообщение: " << message << std::endl;
    return true;
}
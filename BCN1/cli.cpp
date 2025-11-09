#include "frame_config.h"
#include <iostream>
#include <sstream>
#include "io.h"
#include <iomanip>

DWORD select_baud_rate() {
    int choice;
    DWORD baudRates[] = { CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800, CBR_9600, CBR_14400, CBR_19200, CBR_38400, CBR_57600, CBR_115200 };
    int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
    std::cout << "Выберите скорость передачи (Baud Rate):\n";
    for (int i = 0; i < numRates; i++) std::cout << i + 1 << " - " << baudRates[i] << std::endl;
    std::cout << "Введите номер: ";
    std::cin >> choice;
    while ((getchar()) != '\n');
    if (choice >= 1 && choice <= numRates) return baudRates[choice - 1];
    std::cout << "Неверный выбор. Устанавливается 9600\n";
    return CBR_9600;
}

void print_frame_info(const FrameInfo& frame) {
    const std::vector<uint8_t>& raw_frame = frame.raw_frame;
    if (raw_frame.empty()) {
        std::cout << "Кадр пуст.\n";
        return;
    }

    std::cout << "--- Информация о кадре ---\n";
    size_t i = 0;
    std::cout << "Флаг начала кадра: " << to_hex_string(raw_frame[i++]) << "\n";

    auto print_byte_and_advance = [&](size_t& idx) {
        if (raw_frame[idx] == 0x7D && idx + 1 < raw_frame.size()) {
            std::cout << to_hex_string(raw_frame[idx]) << " " << to_hex_string(raw_frame[idx + 1]);
            idx += 2;
        }
        else {
            std::cout << to_hex_string(raw_frame[idx++]);
        }
        };

    std::cout << "Адрес: "; print_byte_and_advance(i); std::cout << "\n";
    std::cout << "Управление: "; print_byte_and_advance(i); std::cout << "\n";
    std::cout << "Счётчик: "; print_byte_and_advance(i); std::cout << "\n";
    std::cout << "Вариант: "; print_byte_and_advance(i); std::cout << "\n";
    i += 2;

    // Данные (payload)
    std::cout << "Данные (" << frame.payload.size() << " байт): ";
    size_t payload_bytes_read = 0;
    size_t start_of_payload_in_raw = i; // Запоминаем, где начинается payload в raw_frame
    for (size_t k = 0; k < frame.payload.size(); ++k) {
        print_byte_and_advance(i);
        std::cout << " ";
    }
    std::cout << "\n";

    // FCS (Hamming parity bits)
    std::cout << "FCS (Последовательность проверки кадра, " << frame.fcs_parity_bits.size() << " байт): ";
    size_t fcs_start_in_raw = i; // Запоминаем, где начинается FCS в raw_frame
    for (size_t k = 0; k < frame.fcs_parity_bits.size(); ++k) {
        print_byte_and_advance(i);
        std::cout << " ";
    }
    std::cout << "\n";

    std::cout << "Флаг конца кадра: " << to_hex_string(raw_frame.back()) << "\n";
}
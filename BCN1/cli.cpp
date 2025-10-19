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

    size_t i = 0;
    std::cout << "Флаг начала кадра: " << to_hex_string(raw_frame[i++]) << "\n";

    auto print_byte = [&](size_t& idx) {
        if (raw_frame[idx] == 0x7D && idx + 1 < raw_frame.size()) {
            std::cout << to_hex_string(raw_frame[idx]) << " " << to_hex_string(raw_frame[idx + 1]);
            idx += 2;
        }
        else {
            std::cout << to_hex_string(raw_frame[idx++]);
        }
        };

    std::cout << "Адрес: "; print_byte(i); std::cout << "\n";
    std::cout << "Управление: "; print_byte(i); std::cout << "\n";
    std::cout << "Счётчик: "; print_byte(i); std::cout << "\n";
    std::cout << "Вариант: "; print_byte(i); std::cout << "\n";
    std::cout << "Длинна поля данных: "; print_byte(i); std::cout << " "; print_byte(i); std::cout << "\n";

    // Данные (payload + FCS)
    std::cout << "Данные, закодированные кодом Хэмминга: ";
    for (; i < raw_frame.size() - 3; ++i) {
        std::cout << to_hex_string(raw_frame[i]) << " ";
    }
    std::cout << "\n";
    std::cout << "FCS: "; print_byte(i); std::cout << " "; print_byte(i); std::cout << "\n";

    std::cout << "Флаг конца кадра: " << to_hex_string(raw_frame.back()) << "\n";

    std::cout << std::endl;
}

void print_frame_info_with_hamming_status(const FrameInfo& frame) {
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
    std::cout << "Длинна поля данных: "; print_byte_and_advance(i); std::cout << " "; print_byte_and_advance(i); std::cout << "\n";

    // Данные (payload)
    std::cout << "Данные (payload, " << frame.payload.size() << " байт): ";
    size_t payload_bytes_read = 0;
    size_t start_of_payload_in_raw = i; // Запоминаем, где начинается payload в raw_frame
    for (size_t k = 0; k < frame.payload.size(); ++k) {
        print_byte_and_advance(i);
        std::cout << " ";
    }
    std::cout << "\n";

    // FCS (Hamming parity bits)
    std::cout << "FCS (Hamming parity bits, " << frame.fcs_parity_bits.size() << " байт): ";
    size_t fcs_start_in_raw = i; // Запоминаем, где начинается FCS в raw_frame
    for (size_t k = 0; k < frame.fcs_parity_bits.size(); ++k) {
        print_byte_and_advance(i);
        std::cout << " ";
    }
    std::cout << "\n";

    std::cout << "Флаг конца кадра: " << to_hex_string(raw_frame.back()) << "\n";

    std::cout << "Статус декодирования Хэмминга:\n";
    std::cout << "  Была исправлена одиночная ошибка: " << (frame.had_single_error ? "Да" : "Нет") << "\n";
    std::cout << "  Была обнаружена двойная ошибка: " << (frame.had_double_error ? "Да" : "Нет") << "\n";
    std::cout << "Кадр валиден (нет двойных ошибок): " << (frame.valid ? "Да" : "Нет") << "\n";
    std::cout << std::endl;
}
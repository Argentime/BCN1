#include "frame_config.h"
#include <iostream>
#include "io.h"

void print_frame_info(const FrameInfo& frame) {
    // Первая строка: логические поля
    std::cout << "| Флаг | Адрес | Управление | Счётчик | Вариант | FCS (hex bytes) | Данные |" << std::endl;

    // Составим представление отдельных частей байт-строки (расщепление raw_frame по полям с учётом escape)
    // Однако frame.raw_frame — это уже stuffed (с флагами). Для наглядности мы распечатаем сырые байты на второй строке.
    // Первая строка — значения полей (логично)
    std::cout << "| " << to_hex_string(frame.raw_frame.empty() ? 0 : frame.raw_frame.front())
        << " | " << to_hex_string(frame.address)
        << " | " << to_hex_string(frame.control)
        << " | " << to_hex_string(frame.sequence)
        << " | " << to_hex_string(frame.variant)
        << " | ";

    // FCS bytes
    if (!frame.fcs_bytes.empty()) {
        for (uint8_t b : frame.fcs_bytes) std::cout << to_hex_string(b) << " ";
    }
    else {
        std::cout << "(none) ";
    }

    std::cout << "| ";

    // данные (логические payload bytes)
    for (uint8_t b : frame.payload) std::cout << to_hex_string(b) << " ";
    std::cout << " |" << std::endl;

    // Вторая строка: сырые байты (точно как прошли/пришли в порт) — raw_frame
    std::cout << "Raw frame bytes (" << frame.raw_frame.size() << "): ";
    for (uint8_t b : frame.raw_frame) std::cout << to_hex_string(b) << " ";
    std::cout << std::endl << std::dec;
}

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

//void print_frame_info(const std::vector<uint8_t>& raw_frame) {
//    size_t i = 0;
//    std::cout << "Флаг начала кадра: " << to_hex_string(raw_frame[i++]) << "\n";
//
//    // --- Адрес ---
//    std::cout << "Адрес: ";
//    if (raw_frame[i] == 0x7D && i + 1 < raw_frame.size()) {
//        std::cout << to_hex_string(raw_frame[i]) << " " << to_hex_string(raw_frame[i + 1]) << "\n";
//        i += 2;
//    }
//    else {
//        std::cout << to_hex_string(raw_frame[i++]) << "\n";
//    }
//
//    // --- Управление ---
//    std::cout << "Управление: ";
//    if (raw_frame[i] == 0x7D && i + 1 < raw_frame.size()) {
//        std::cout << to_hex_string(raw_frame[i]) << " " << to_hex_string(raw_frame[i + 1]) << "\n";
//        i += 2;
//    }
//    else {
//        std::cout << to_hex_string(raw_frame[i++]) << "\n";
//    }
//
//    // --- Счётчик ---
//    std::cout << "Счётчик: ";
//    if (raw_frame[i] == 0x7D && i + 1 < raw_frame.size()) {
//        std::cout << to_hex_string(raw_frame[i]) << " " << to_hex_string(raw_frame[i + 1]) << "\n";
//        i += 2;
//    }
//    else {
//        std::cout << to_hex_string(raw_frame[i++]) << "\n";
//    }
//
//    // --- Вариант ---
//    std::cout << "Вариант: ";
//    if (raw_frame[i] == 0x7D && i + 1 < raw_frame.size()) {
//        std::cout << to_hex_string(raw_frame[i]) << " " << to_hex_string(raw_frame[i + 1]) << "\n";
//        i += 2;
//    }
//    else {
//        std::cout << to_hex_string(raw_frame[i++]) << "\n";
//    }
//
//    // --- Данные ---
//    std::cout << "Данные: ";
//    for (; i < raw_frame.size() - 1; ++i) {
//        std::cout << to_hex_string(raw_frame[i]) << " ";
//    }
//    std::cout << "\n";
//
//    // --- Конечный флаг ---
//    std::cout << "Флаг конца кадра: " << to_hex_string(raw_frame.back()) << "\n\n";
//}
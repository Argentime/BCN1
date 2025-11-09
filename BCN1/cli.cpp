#include "cli.h"
#include "frame_config.h"
#include "io.h"
#include <iostream>
#include <iomanip>
#include <mutex>

// Подключаем глобальный мьютекс
extern std::mutex g_cout_mutex;

DWORD select_baud_rate() {
    // Весь вывод защищен мьютексом
    std::lock_guard<std::mutex> lock(g_cout_mutex);

    int choice;
    DWORD baudRates[] = { CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800, CBR_9600, CBR_14400, CBR_19200, CBR_38400, CBR_57600, CBR_115200 };
    int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
    std::cout << "Выберите скорость передачи (Baud Rate):\n";
    for (int i = 0; i < numRates; i++) std::cout << i + 1 << " - " << baudRates[i] << std::endl;

    std::string input;
    std::cout << "Введите номер: ";
    // Чтение не блокирует другие потоки, но вывод должен быть атомарным
    // Для простоты, оставим так, но в сложных GUI это делается асинхронно
    std::cin >> input;
  
    try {
        choice = std::stoi(input);
        if (choice >= 1 && choice <= numRates) return baudRates[choice - 1];
    }
    catch (...) {
        // Игнорируем ошибку
    }

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
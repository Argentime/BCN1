#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "main.h"
#include <algorithm> // для std::min
#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <locale.h>
#include <stdbool.h>
#include <limits>
#include <sstream>
#include <iomanip>

HANDLE hComm1;
HANDLE hComm2;

#define COM_PORT1 L"COM2"
#define COM_PORT2 L"COM4"

FrameInfo last_sent_frame;
FrameInfo last_received_frame;

void print_last_error() {
    DWORD dwError = GetLastError();
    std::cout << "Ошибка: " << dwError << std::endl;
}

std::string WCharToString(LPCWSTR wstr) {
    if (!wstr) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str_to[0], size_needed, NULL, NULL);
    return str_to;
}

// --- CRC16-CCITT (0x1021, init 0xFFFF) ---
uint16_t crc16_ccitt(const std::vector<uint8_t>& data) {
    uint16_t crc = 0xFFFF;
    for (uint8_t b : data) {
        crc ^= (uint16_t)b << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

HANDLE open_com_port(LPCWSTR COM_PORT) {
    HANDLE hComm = CreateFile(COM_PORT, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hComm == INVALID_HANDLE_VALUE) {
        std::cout << "Не удалось открыть порт " << WCharToString(COM_PORT) << "\n";
        print_last_error();
        return NULL;
    }
    return hComm;
}

void configure_com_port(HANDLE hComm, DWORD baudRate) {
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hComm, &dcbSerialParams)) { print_last_error(); return; }
    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hComm, &dcbSerialParams)) { print_last_error(); return; }
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 1000;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 100;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hComm, &timeouts);
}

// Функции для работы с кадрами
std::vector<uint8_t> build_information_field(uint8_t address, uint8_t control, uint8_t sequence, uint8_t variant, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> info;
    info.push_back(address);
    info.push_back(control);
    info.push_back(sequence);
    info.push_back(variant);
    info.insert(info.end(), payload.begin(), payload.end());
    return info;
}

void parse_information_field(const std::vector<uint8_t>& info, FrameInfo& frame) {
    if (info.size() < 4) return;
    frame.address = info[0];
    frame.control = info[1];
    frame.sequence = info[2];
    frame.variant = info[3];
    frame.payload.assign(info.begin() + 4, info.end());
}

std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> stuffed;
    for (uint8_t byte : data) {
        if (byte == 0x7E || byte == 0x7D) {
            stuffed.push_back(0x7D);
            stuffed.push_back(byte ^ 0x20);
        }
        else {
            stuffed.push_back(byte);
        }
    }
    stuffed.insert(stuffed.begin(), 0x7E);
    stuffed.push_back(0x7E);
    return stuffed;
}

std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> unstuffed;
    for (size_t i = 1; i < data.size() - 1; i++) {
        if (data[i] == 0x7D && i + 1 < data.size() - 1) {
            unstuffed.push_back(data[i + 1] ^ 0x20);
            i++;
        }
        else {
            unstuffed.push_back(data[i]);
        }
    }
    return unstuffed;
}

// запись в COM
bool write_to_port(HANDLE hComm, const std::vector<uint8_t>& data) {
    DWORD bytesWritten;
    if (!WriteFile(hComm, data.data(), (DWORD)data.size(), &bytesWritten, NULL) || bytesWritten != data.size()) {
        return false;
    }
    return true;
}

// чтение из COM
bool read_from_port(HANDLE hComm, std::vector<uint8_t>& buffer) {
    DWORD bytesRead;
    char temp[256] = { 0 };

    COMSTAT com_stat;
    DWORD dw_error;
    if (!ClearCommError(hComm, &dw_error, &com_stat)) {
        return false;
    }

    if (com_stat.cbInQue == 0) return false;

    if (ReadFile(hComm, temp, com_stat.cbInQue, &bytesRead, NULL)) {
        buffer.assign(temp, temp + bytesRead);
        return true;
    }
    return false;
}

// отправка сообщения в кадрах фиксированного размера
bool send_string_as_frame(HANDLE hComm, const std::string& message, uint8_t& sequence,
    uint8_t address = 0x01, uint8_t control = 0x00, uint8_t variant = 0x00) {
    const size_t MAX_PAYLOAD_SIZE = 64;
    size_t offset = 0;
    while (offset < message.size()) {
        size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size() - offset);
        std::vector<uint8_t> payload(message.begin() + offset, message.begin() + offset + chunk_size);

        std::vector<uint8_t> info = build_information_field(address, control, sequence, variant, payload);
        std::vector<uint8_t> stuffed = byte_stuff(info);

        parse_information_field(info, last_sent_frame);
        last_sent_frame.raw_frame = stuffed;

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

// получение кадра
bool receive_frame(HANDLE hComm) {
    std::vector<uint8_t> full_message; // для вывода всего сообщения
    char byte;
    DWORD bytesRead;
    std::vector<uint8_t> buffer;
    bool started = false;

    while (true) {
        COMSTAT comStat;
        DWORD dwError;
        ClearCommError(hComm, &dwError, &comStat);

        if (comStat.cbInQue == 0) break;

        if (!ReadFile(hComm, &byte, 1, &bytesRead, NULL) || bytesRead == 0)
            continue;

        uint8_t b = static_cast<uint8_t>(byte);

        if (b == 0x7E) {
            if (!started) {
                started = true;
                buffer.clear();
                buffer.push_back(b);
            }
            else {
                buffer.push_back(b);
                std::vector<uint8_t> unstuffed = byte_unstuff(buffer);
                FrameInfo frame;
                parse_information_field(unstuffed, frame);

                // сохраняем **последний кадр**
                last_received_frame = frame;
                last_received_frame.raw_frame = buffer;

                // добавляем payload к общей строке
                full_message.insert(full_message.end(), frame.payload.begin(), frame.payload.end());

                started = false;
                buffer.clear();
            }
        }
        else if (started) {
            buffer.push_back(b);
        }
    }

    if (!full_message.empty()) {
        std::string message(full_message.begin(), full_message.end());
        std::cout << "Принято сообщение: " << message << std::endl;
    }
    else {
        std::cout << "Нет данных для чтения." << std::endl;
    }

    return !full_message.empty();
}

std::string to_hex_string(uint8_t value) {
    std::ostringstream oss;
    oss << "0x"
        << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(value);
    return oss.str();
}

// Печать кадра в двух видах
void print_frame_info(const std::vector<uint8_t>& raw_frame) {
    // Первая строка — логическая структура кадра
    std::cout << "| Флаг | Адрес | Управление | Счётчик | Вариант | Данные | Флаг |\n";

    // Вторая строка — байты кадра
    for (uint8_t b : raw_frame) {
        std::cout << std::setw(6) << to_hex_string(b);
    }
    std::cout << "\n";
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

int main() {
    setlocale(LC_ALL, "Russian");
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);

    DWORD baudRate = CBR_9600;

    hComm1 = open_com_port(COM_PORT1);
    hComm2 = open_com_port(COM_PORT2);
    if (hComm1 == NULL || hComm2 == NULL) {
        if (hComm1 != NULL) CloseHandle(hComm1);
        if (hComm2 != NULL) CloseHandle(hComm2);
        std::cout << "Не удалось открыть один или оба COM-порта.\n";
        return 1;
    }

    configure_com_port(hComm1, baudRate);
    configure_com_port(hComm2, baudRate);
    std::cout << "Порты настроены. Скорость: " << baudRate << std::endl;

    std::string message;
    int choice = -1;
    uint8_t seq = 0;
    uint8_t variant = 0x04;

    while (true) {
        std::cout << "\nМеню:\n";
        std::cout << "1 - Отправить сообщение\n";
        std::cout << "2 - Прочитать сообщение\n";
        std::cout << "3 - Установить скорость передачи\n";
        std::cout << "4 - Просмотр последнего отправленного кадра\n";
        std::cout << "5 - Просмотр последнего принятого кадра\n";
        std::cout << "0 - Выход\n";
        std::cout << "Ваш выбор: ";
        std::cin >> choice;

        while (getchar() != '\n');

        switch (choice) {
        case 1:
            std::cout << "Введите сообщение: ";
            std::getline(std::cin, message);
            send_string_as_frame(hComm1, message, seq, 0x01, 0x00, variant);
            break;
        case 2:
            receive_frame(hComm2);
            break;
        case 3:
            baudRate = select_baud_rate();
            configure_com_port(hComm1, baudRate);
            configure_com_port(hComm2, baudRate);
            break;
        case 4:
            std::cout << "Последний отправленный кадр:\n";
            print_frame_info(last_sent_frame.raw_frame);
            break;
        case 5:
            std::cout << "Последний принятый кадр:\n";
            print_frame_info(last_received_frame.raw_frame);
            break;
        case 0:
            CloseHandle(hComm1);
            CloseHandle(hComm2);
            std::cout << "Выход.\n";
            return 0;
        default:
            std::cout << "Неверный выбор.\n";
            break;
        }
    }
}

#include "com_config.h"
#include <iostream>
#define NOMINMAX

HANDLE open_com_port(LPCWSTR COM_PORT) {
    HANDLE hComm = CreateFile(
        COM_PORT,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hComm == INVALID_HANDLE_VALUE) {
        std::cout << "Ошибка открытия COM-порта.\n";
        return NULL;
    }
    return hComm;
}

void configure_com_port(HANDLE hComm, DWORD baudRate) {
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hComm, &dcbSerialParams)) {
        std::cout << "Ошибка получения состояния COM-порта.\n";
        return;
    }
    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hComm, &dcbSerialParams)) {
        std::cout << "Ошибка установки состояния COM-порта.\n";
        return;
    }

    // --- ДОБАВЛЕН БЛОК УСТАНОВКИ ТАЙМАУТОВ ---
    // Это критически важно для CSMA/CD, чтобы чтение не блокировало программу.
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = 50; // Ждем ответа не более 50 мс
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        std::cout << "Ошибка установки таймаутов COM-порта.\n";
    }
}

bool write_to_port(HANDLE hComm, const std::vector<uint8_t>& data) {
    DWORD bytesWritten;
    if (!WriteFile(hComm, data.data(), data.size(), &bytesWritten, NULL)) {
        return false;
    }
    return bytesWritten == data.size();
}

bool read_from_port(HANDLE hComm, std::vector<uint8_t>& buffer) {
    // Будем читать по одному байту, чтобы не ждать заполнения буфера
    char byte_char;
    DWORD bytesRead;
    buffer.clear();

    // Читаем первый байт
    if (ReadFile(hComm, &byte_char, 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer.push_back(static_cast<uint8_t>(byte_char));

        // Проверяем, есть ли еще данные в буфере порта, и читаем их
        DWORD errors;
        COMSTAT stat;
        if (ClearCommError(hComm, &errors, &stat) && stat.cbInQue > 0) {
            std::vector<char> temp_buf(stat.cbInQue);
            if (ReadFile(hComm, temp_buf.data(), stat.cbInQue, &bytesRead, NULL)) {
                for (DWORD i = 0; i < bytesRead; ++i) {
                    buffer.push_back(static_cast<uint8_t>(temp_buf[i]));
                }
            }
        }
        return true;
    }
    return false;
}
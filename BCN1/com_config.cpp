#include <wtypes.h>
#include <iostream>
#include <vector>
#include "io.h"

HANDLE open_com_port(LPCWSTR COM_PORT) {
    HANDLE hComm = CreateFile(COM_PORT, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hComm == INVALID_HANDLE_VALUE) {
        std::cout << "Ќе удалось открыть порт " << WCharToString(COM_PORT) << "\n";
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

bool write_to_port(HANDLE hComm, const std::vector<uint8_t>& data) {
    DWORD bytesWritten;
    if (!WriteFile(hComm, data.data(), (DWORD)data.size(), &bytesWritten, NULL) || bytesWritten != data.size()) {
        return false;
    }
    return true;
}

// чтение из COM (байтовое чтение)
bool read_from_port(HANDLE hComm, std::vector<uint8_t>& buffer) {
    DWORD bytesRead;
    char temp[512] = { 0 };

    COMSTAT com_stat;
    DWORD dw_error;
    if (!ClearCommError(hComm, &dw_error, &com_stat)) {
        return false;
    }

    if (com_stat.cbInQue == 0) return false;

    DWORD toRead = com_stat.cbInQue;
    if (toRead > sizeof(temp)) toRead = sizeof(temp);

    if (ReadFile(hComm, temp, toRead, &bytesRead, NULL)) {
        buffer.assign((uint8_t*)temp, (uint8_t*)temp + bytesRead);
        return true;
    }
    return false;
}
#include <string>
#include <iomanip>
#include <sstream>
#include <wtypes.h>
#include <iostream>
#include <vector>

std::string to_hex_string(uint8_t value) {
    std::ostringstream oss;
    oss << "0x"
        << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(value);
    return oss.str();
}

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

std::string WCharToString(LPCWSTR wstr) {
    if (!wstr) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str_to[0], size_needed, NULL, NULL);
    return str_to;
}

void print_last_error() {
    DWORD dwError = GetLastError();
    std::cout << "Îøèáêà: " << dwError << std::endl;
}

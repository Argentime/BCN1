#pragma once
#include <string>
#include <wtypes.h>
#include <iostream>
#include <vector>

std::string to_hex_string(uint8_t value);
uint16_t crc16_ccitt(const std::vector<uint8_t>& data);
std::string WCharToString(LPCWSTR wstr);
void print_last_error();
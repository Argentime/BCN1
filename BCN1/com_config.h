#pragma once
#include <wtypes.h>
#include <iostream>
#include <vector>

HANDLE open_com_port(LPCWSTR COM_PORT);
void configure_com_port(HANDLE hComm, DWORD baudRate);
bool write_to_port(HANDLE hComm, const std::vector<uint8_t>& data);
bool read_from_port(HANDLE hComm, std::vector<uint8_t>& buffer);
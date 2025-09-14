#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <locale.h>
#include <stdbool.h>
#include <limits>

void print_last_error();
HANDLE open_com_port(LPCWSTR COM_PORT);
void configure_com_port(HANDLE hComm, DWORD baudRate);
void send_string(HANDLE hComm, const std::string& data);
void receive_and_print_string(HANDLE hComm);
DWORD select_baud_rate();
std::string WCharToString(LPCWSTR wstr);
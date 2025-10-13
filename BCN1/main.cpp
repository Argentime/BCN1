#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "main.h"
#include "hamming.h"
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
#include "com_config.h"
#include "cli.h"
#include "send.h"
#include "receive.h"

HANDLE hComm1;
HANDLE hComm2;
FrameInfo last_sent_frame;

#define COM_PORT1 L"COM2"
#define COM_PORT2 L"COM4"

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
        std::cout << "Выход из программы... ";
        system("pause");
        return 1;
    }

    configure_com_port(hComm1, baudRate);
    configure_com_port(hComm2, baudRate);
    std::cout << "Порты настроены. Скорость: " << baudRate << std::endl;

    std::string message;
    std::string input;
    int choice = -1;
    uint8_t seq = 0;
    uint8_t variant = 0x03;

    while (choice!=0) {
        std::cout << "\nМеню:\n";
        std::cout << "1 - Отправить сообщение\n";
        std::cout << "2 - Прочитать сообщение\n";
        std::cout << "3 - Установить скорость передачи\n";
        std::cout << "4 - Просмотр последнего отправленного кадра\n";
        std::cout << "0 - Выход из программы\n";
        std::cout << "Ваш выбор: ";
        std::getline(std::cin, input);

        bool is_number = !input.empty() && std::all_of(input.begin(), input.end(), ::isdigit);
        if (!is_number) {
            std::cout << "Ошибка: нужно ввести число от 0 до 5!\n";
            continue;
        }

        choice = std::stoi(input);

        switch (choice) {
        case 1: {
            std::cout << "Введите сообщение: ";
            std::getline(std::cin, message);
            send_string_as_frame(last_sent_frame, hComm1, message, seq, 0x01, 0x00, variant);
            break;
        }
        case 2: {
            receive_frame(hComm2);
            break;
        }
        case 3: {
            baudRate = select_baud_rate();
            configure_com_port(hComm1, baudRate);
            configure_com_port(hComm2, baudRate);
            break;
        }
        case 4: {
            std::cout << "Последний отправленный кадр:\n";
            print_frame_info(last_sent_frame);
            break;
        }
        case 0: {
            CloseHandle(hComm1);
            CloseHandle(hComm2);
            break;
        }
        default: {
            std::cout << "Неверный выбор.\n";
            break;
        }       
        }
    }
    std::cout << "Выход из программы... ";
    system("pause");
    return 0;
}

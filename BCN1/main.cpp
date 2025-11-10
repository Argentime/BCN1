#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "hamming.h"
#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <locale.h>
#include <stdbool.h>
#include <limits>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue> // Для очереди сообщений

#include "com_config.h"
#include "csma_config.h"
#include "send.h"
#include "receive.h"
#include "cli.h"

// --- Глобальные переменные ---
HANDLE hComm1;
HANDLE hComm2;
FrameInfo last_sent_frame;

std::mutex g_cout_mutex;
std::atomic<bool> g_stop_thread(false);

// Очередь для принятых сообщений
std::queue<std::string> g_received_messages;
std::mutex g_message_queue_mutex;

#define COM_PORT1 L"COM2"
#define COM_PORT2 L"COM4"

int main() {
    setlocale(LC_ALL, "Russian");
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);

    srand((unsigned)time(NULL));

    std::random_device rd;
    std::mt19937 rng((unsigned)time(NULL) ^ rd());

    DWORD baudRate = CBR_9600;

    hComm1 = open_com_port(COM_PORT1);
    hComm2 = open_com_port(COM_PORT2);
    if (hComm1 == NULL || hComm2 == NULL) {
        if (hComm1 != NULL) CloseHandle(hComm1);
        if (hComm2 != NULL) CloseHandle(hComm2);
        std::cout << "Не удалось открыть один или оба COM-порта. Выход из программы... Нажмите Enter для продолжения\n";
        _fgetchar();
        return 1;
    }

    configure_com_port(hComm1, baudRate);
    configure_com_port(hComm2, baudRate);
    std::cout << "Порты настроены. Скорость: " << baudRate << std::endl;

    std::thread receiver_thread(receiver_emulator_thread_func, hComm2, std::ref(rng));
    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "Фоновый поток приёмника-эмулятора запущен.\n";
    }

    std::string message;
    std::string input;
    int choice = -1;
    uint8_t seq = 0;
    uint8_t variant = 0x03;

    while (choice != 0) {
        {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "\nМеню:\n";
            std::cout << "1 - Отправить сообщение\n";
            std::cout << "2 - Прочитать новые сообщения\n";
            std::cout << "3 - Вкл/Выкл подробный лог коллизий (сейчас: " << (g_dynamic_info_enabled ? "ВКЛ" : "ВЫКЛ") << ")\n";
            std::cout << "4 - Посмотреть статистику коллизий\n";
            std::cout << "5 - Установить скорость передачи\n";
            std::cout << "6 - Просмотр последнего отправленного кадра\n";
            std::cout << "0 - Выход из программы\n";
            std::cout << "Ваш выбор: ";
        }

        std::getline(std::cin, input);

        try {
            if (!input.empty()) choice = std::stoi(input);
            else choice = -1;
        }
        catch (...) {
            choice = -1;
        }

        switch (choice) {
        case 1: {
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "Введите сообщение: ";
            }
            std::getline(std::cin, message);
            send_with_csma_cd(last_sent_frame, hComm1, message, seq, 0x01, 0x00, variant);
            break;
        }
        case 2: {
            std::lock_guard<std::mutex> lock(g_message_queue_mutex);
            std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
            if (g_received_messages.empty()) {
                std::cout << "Нет новых сообщений.\n";
            }
            else {
                std::cout << "--- Новые сообщения ---\n";
                while (!g_received_messages.empty()) {
                    std::cout << g_received_messages.front() << std::endl;
                    g_received_messages.pop();
                }
            }
            break;
        }
        case 3: {
            g_dynamic_info_enabled = !g_dynamic_info_enabled;
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "Подробный лог теперь " << (g_dynamic_info_enabled ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") << ".\n";
            break;
        }
        case 4: {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "--- Статистика ---\n";
            std::cout << "Обнаружено коллизий с момента запуска: " << g_collision_count << std::endl;
            break;
        }
        case 5: {
            baudRate = select_baud_rate();
            configure_com_port(hComm1, baudRate);
            configure_com_port(hComm2, baudRate);
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "Скорость портов обновлена до " << baudRate << std::endl;
            break;
        }
        case 6: {
            print_frame_info(last_sent_frame);
            break;
        }
        case 0: break;
        default: {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "Неверный выбор.\n";
            break;
        }
        }
    }

    g_stop_thread = true;
    receiver_thread.join();

    CloseHandle(hComm1);
    CloseHandle(hComm2);

    std::cout << "Выход из программы... Нажмите Enter для продолжения";
    _fgetchar();
    return 0;
}
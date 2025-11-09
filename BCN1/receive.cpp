#include "receive.h"
#include "frame_config.h"
#include "csma_config.h"
#include "com_config.h"
#include "io.h"
#include <vector>
#include <iostream>
#include <random>
#include <mutex>
#include <thread>
#include <chrono>

// Подключаем внешние глобальные переменные
extern std::mutex g_cout_mutex;
extern std::atomic<bool> g_stop_thread;

// Прототип функции печати из cli.cpp
void print_frame_info_with_hamming_status(
    const FrameInfo& frame,
    const std::vector<uint8_t>& received_payload,
    const std::vector<uint8_t>& received_parity_bits
);

void receiver_emulator_thread_func(HANDLE hComm, std::mt19937& rng) {
    std::vector<uint8_t> buffer;
    bool in_frame_reception = false;

    while (!g_stop_thread) {
        std::vector<uint8_t> received_byte_vec;
        if (!read_from_port(hComm, received_byte_vec) || received_byte_vec.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint8_t received_byte = received_byte_vec[0];
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        if (!in_frame_reception) {
            if (received_byte == ENQ) {
                if (dist(rng) < PROBABILITY_CHANNEL_BUSY) {
                    {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[ЭМУЛЯТОР] Канал занят. Отправка NAK.\n";
                    }
                    write_to_port(hComm, { NAK });
                }
                else {
                    {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[ЭМУЛЯТОР] Канал свободен. Отправка ACK.\n";
                    }
                    write_to_port(hComm, { ACK });
                    in_frame_reception = true;
                    buffer.clear();
                }
            }
        }
        else {
            if (received_byte == JAM_SIGNAL) {
                {
                    std::lock_guard<std::mutex> lock(g_cout_mutex);
                    std::cout << "[ЭМУЛЯТОР] Получен Jam-сигнал. Сброс.\n";
                }
                in_frame_reception = false;
                buffer.clear();
                continue;
            }

            if (dist(rng) < PROBABILITY_COLLISION) {
                {
                    std::lock_guard<std::mutex> lock(g_cout_mutex);
                    std::cout << "[ЭМУЛЯТОР] Коллизия! Отправка COL.\n";
                }
                write_to_port(hComm, { COL });
                in_frame_reception = false;
                buffer.clear();
            }
            else {
                buffer.push_back(received_byte);

                if (received_byte == FLAG && buffer.size() > 1 && buffer[0] == FLAG) {
                    {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "\n[ПРИЁМНИК] Получен полный кадр. Декодирование...\n";
                    }

                    std::vector<uint8_t> unstuffed = byte_unstuff(buffer);
                    FrameInfo frameTemp;
                    if (parse_information_field_with_dynamic_fcs(unstuffed, frameTemp)) {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "--- ПРИНЯТО СООБЩЕНИЕ ---\n";
                        std::string msg(frameTemp.payload.begin(), frameTemp.payload.end());
                        std::cout << "Сообщение: \"" << msg << "\"\n";
                        std::cout << "Статус: " << (frameTemp.valid ? "Валидно" : "Невалидно")
                            << ", одиночных ошибок: " << (frameTemp.had_single_error ? "Да" : "Нет")
                            << ", двойных ошибок: " << (frameTemp.had_double_error ? "Да" : "Нет") << "\n";
                        std::cout << "---------------------------\n";
                    }
                    else {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[ПРИЁМНИК] Ошибка разбора кадра.\n";
                    }

                    in_frame_reception = false;
                    buffer.clear();
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(g_cout_mutex);
    std::cout << "Фоновый поток приёмника-эмулятора остановлен.\n";
}
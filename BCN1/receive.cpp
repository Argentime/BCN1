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
#include <queue>

// Подключаем внешние глобальные переменные
extern std::mutex g_cout_mutex;
extern std::atomic<bool> g_stop_thread;
extern std::atomic<bool> g_dynamic_info_enabled;

// Очередь для принятых сообщений и её мьютекс
extern std::queue<std::string> g_received_messages;
extern std::mutex g_message_queue_mutex;

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
                    if (g_dynamic_info_enabled) {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[КАНАЛ] Среда занята (эмуляция).\n";
                    }
                    write_to_port(hComm, { NAK });
                }
                else {
                    if (g_dynamic_info_enabled) {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[КАНАЛ] Среда свободна, передача разрешена.\n";
                    }
                    write_to_port(hComm, { ACK });
                    in_frame_reception = true;
                    buffer.clear();
                }
            }
        }
        else {
            if (received_byte == JAM_SIGNAL) {
                if (g_dynamic_info_enabled) {
                    std::lock_guard<std::mutex> lock(g_cout_mutex);
                    std::cout << "[КАНАЛ] Получен Jam-сигнал, прием прерван.\n";
                }
                in_frame_reception = false;
                buffer.clear();
                continue;
            }

            if (dist(rng) < PROBABILITY_COLLISION) {
                if (g_dynamic_info_enabled) {
                    std::lock_guard<std::mutex> lock(g_cout_mutex);
                    std::cout << "[КАНАЛ] Коллизия в среде (эмуляция)!\n";
                }
                write_to_port(hComm, { COL });
                in_frame_reception = false;
                buffer.clear();
            }
            else {
                buffer.push_back(received_byte);

                if (received_byte == FLAG && buffer.size() > 1 && buffer[0] == FLAG) {
                    if (g_dynamic_info_enabled) {
                        std::lock_guard<std::mutex> lock(g_cout_mutex);
                        std::cout << "[ПРИЁМНИК] Получен полный кадр. Декодирование...\n";
                    }

                    std::vector<uint8_t> unstuffed = byte_unstuff(buffer);
                    FrameInfo frameTemp;
                    if (parse_information_field_with_dynamic_fcs(unstuffed, frameTemp)) {
                        std::string msg(frameTemp.payload.begin(), frameTemp.payload.end());

                        // Помещаем сообщение в очередь
                        std::lock_guard<std::mutex> lock(g_message_queue_mutex);
                        g_received_messages.push(msg);

                        if (g_dynamic_info_enabled) {
                            std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
                            std::cout << "[ПРИЁМНИК] Сообщение \"" << msg << "\" принято и помещено в очередь.\n";
                        }
                    }
                    else if (g_dynamic_info_enabled) {
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
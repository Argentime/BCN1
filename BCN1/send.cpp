#define NOMINMAX
#include "send.h"
#include "com_config.h"
#include "csma_config.h"
#include "hamming.h"
#include "io.h"
#include <iostream>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <windows.h>

extern std::mutex g_cout_mutex;
extern std::atomic<bool> g_dynamic_info_enabled;

void send_jam_signal(HANDLE hComm) {
    std::vector<uint8_t> jam(JAM_SIGNAL_LENGTH, JAM_SIGNAL);
    write_to_port(hComm, jam);
    if (g_dynamic_info_enabled) {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "[КАНАЛ] Отправка Jam-сигнала для усиления коллизии...\n";
    }
}

bool send_with_csma_cd(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message,
    uint8_t& sequence, uint8_t address, uint8_t control, uint8_t variant) {

    if (message.empty()) {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "Сообщение не может быть пустым.\n";
        return false;
    }

    const size_t MAX_PAYLOAD_SIZE = 64;
    size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size());
    std::vector<uint8_t> payload_chunk(message.begin(), message.begin() + chunk_size);
    std::vector<uint8_t> parity_bits = hamming_generate_parity_bits(payload_chunk);
    std::vector<uint8_t> info_field = build_information_field_with_dynamic_fcs(address, control, sequence, variant, payload_chunk, parity_bits);
    std::vector<uint8_t> stuffed_frame = byte_stuff(info_field);

    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "Начинается процесс отправки сообщения...\n";
    }

    int n = 0;
    while (n < MAX_ATTEMPTS) {
        if (g_dynamic_info_enabled) {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "[ПЕРЕДАТЧИК] Попытка " << n + 1 << ". Прослушивание канала...\n";
        }
        write_to_port(hComm, { ENQ });

        std::vector<uint8_t> response;
        read_from_port(hComm, response);

        if (response.empty() || response[0] != ACK) {
            if (g_dynamic_info_enabled) {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "[ПЕРЕДАТЧИК] Канал занят, ожидание...\n";
            }
            Sleep(SLOT_TIME_MS);
            continue;
        }

        if (g_dynamic_info_enabled) {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "[ПЕРЕДАТЧИК] Канал свободен. Начало побайтовой передачи...\n";
        }

        bool collision_detected = false;
        for (uint8_t byte_to_send : stuffed_frame) {
            write_to_port(hComm, { byte_to_send });
            read_from_port(hComm, response);
            if (!response.empty() && response[0] == COL) {
                g_collision_count++;
                collision_detected = true;
                break;
            }
        }

        if (collision_detected) {
            n++;
            send_jam_signal(hComm);
            int k = std::min(n, 10);

            // Вычисляем верхнюю границу для случайного числа: 2^k.
            int max_rand_range = static_cast<int>(pow(2, k));

            // Генерируем случайное число 'r' в диапазоне [0, 2^k - 1].
            if (max_rand_range == 0) max_rand_range = 1;
            int r = rand() % max_rand_range;

            DWORD backoff_delay = r * SLOT_TIME_MS;

            backoff_delay = std::min(backoff_delay, (DWORD)MAX_BACKOFF_DELAY_MS);
            if (g_dynamic_info_enabled) {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "[ПЕРЕДАТЧИК] Обнаружена коллизия. Повторная попытка через " << backoff_delay << " мс.\n";
            }
            Sleep(backoff_delay);
        }
        else {
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "Сообщение успешно передано.\n";
            }
            parse_information_field_with_dynamic_fcs(info_field, last_sent_frame);
            last_sent_frame.raw_frame = stuffed_frame;
            sequence++;
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "Ошибка: превышено максимальное количество попыток передачи (" << MAX_ATTEMPTS << ").\n";
    }
    return false;
}
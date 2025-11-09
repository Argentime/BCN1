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
#include <windows.h> // Для Sleep


// Подключаем глобальный мьютекс для вывода в консоль
extern std::mutex g_cout_mutex;

// Вспомогательная функция для отправки jam-сигнала
void send_jam_signal(HANDLE hComm) {
    std::vector<uint8_t> jam(JAM_SIGNAL_LENGTH, JAM_SIGNAL);
    write_to_port(hComm, jam);
    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "[ПЕРЕДАТЧИК] КОЛЛИЗИЯ! Отправка Jam-сигнала...\n";
    }
}

// Главная функция отправки с логикой CSMA/CD
bool send_with_csma_cd(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message,
    uint8_t& sequence, uint8_t address, uint8_t control, uint8_t variant) {

    if (message.empty()) {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "[ПЕРЕДАТЧИК] Сообщение не может быть пустым.\n";
        return false;
    }

    // ... (код формирования кадра остается без изменений) ...
    const size_t MAX_PAYLOAD_SIZE = 64;
    size_t chunk_size = std::min((size_t)MAX_PAYLOAD_SIZE, message.size());
    std::vector<uint8_t> payload_chunk(message.begin(), message.begin() + chunk_size);
    std::vector<uint8_t> parity_bits = hamming_generate_parity_bits(payload_chunk);
    std::vector<uint8_t> info_field = build_information_field_with_dynamic_fcs(
        address, control, sequence, variant, payload_chunk, parity_bits
    );
    std::vector<uint8_t> stuffed_frame = byte_stuff(info_field);

    // --- ИЗМЕНЕНИЕ ЗДЕСЬ: Заменяем цикл for на while ---
    // Счетчик попыток 'n' теперь инкрементируется вручную только при коллизии
    int n = 0;
    while (n < MAX_ATTEMPTS) {
        // --- ШАГ 1: ПРОСЛУШИВАНИЕ КАНАЛА ---
        {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            // Показываем текущую попытку (n+1), но n инкрементируется только после коллизии
            std::cout << "[ПЕРЕДАТЧИК] Попытка " << n + 1 << ". Прослушивание канала...\n";
        }
        write_to_port(hComm, { ENQ });

        std::vector<uint8_t> response;
        read_from_port(hComm, response);

        // Если канал занят, мы просто ждем и повторяем прослушивание
        // на СЛЕДУЮЩЕЙ итерации цикла while, НЕ увеличивая счетчик 'n'.
        if (response.empty() || response[0] != ACK) {
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "[ПЕРЕДАТЧИК] Канал занят. Ожидание (defer)...\n";
            }
            Sleep(SLOT_TIME_MS); // Короткая пауза перед повторным прослушиванием
            continue; // Переходим к следующей итерации while, n не меняется
        }

        // Если мы здесь, значит канал свободен
        {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << "[ПЕРЕДАТЧИК] Канал свободен. Начало передачи...\n";
        }

        // --- ШАГ 2: ПЕРЕДАЧА И ОБНАРУЖЕНИЕ КОЛЛИЗИЙ ---
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

        // --- ШАГ 3: РЕАКЦИЯ НА КОЛЛИЗИЮ ИЛИ УСПЕХ ---
        if (collision_detected) {
            send_jam_signal(hComm);

            // --- ИЗМЕНЕНИЕ ЗДЕСЬ: Инкрементируем счетчик только СЕЙЧАС ---
            n++;

            DWORD backoff_delay = rand() % (MAX_BACKOFF_DELAY_MS + 1);
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "[ПЕРЕДАТЧИК] Задержка (" << n << "-я коллизия) перед следующей попыткой: " << backoff_delay << " мс.\n";
            }
            Sleep(backoff_delay);
            // continue; // Переходим к следующей итерации while, чтобы начать новую попытку
        }
        else {
            // Коллизий не было, передача успешна
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "[ПЕРЕДАТЧИК] Кадр успешно отправлен.\n";
            }
            parse_information_field_with_dynamic_fcs(info_field, last_sent_frame);
            last_sent_frame.raw_frame = stuffed_frame;
            sequence++;
            return true; // Выходим из функции
        }
    }

    // Если цикл while завершился, значит мы превысили MAX_ATTEMPTS
    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "[ПЕРЕДАТЧИК] Ошибка: превышено максимальное количество попыток передачи (" << MAX_ATTEMPTS << ").\n";
    }
    return false;
}
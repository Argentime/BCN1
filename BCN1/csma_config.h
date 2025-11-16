#pragma once
#include <atomic>

// --- Управляющие символы для эмуляции CSMA/CD ---
#define ENQ 0x05 // Enquiry - Запрос на передачу (Carrier Sense)
#define ACK 0x06 // Acknowledge - Канал свободен (Clear to Send)
#define NAK 0x15 // Negative Acknowledge - Канал занят
#define COL 0x18 // Collision - Сигнал коллизии
#define JAM_SIGNAL 0xAA // Байт для jam-последовательности

// --- Параметры алгоритма CSMA/CD ---
#define MAX_ATTEMPTS 16
#define SLOT_TIME_MS 50
#define JAM_SIGNAL_LENGTH 4
#define MAX_BACKOFF_DELAY_MS 2000

// --- Вероятности для эмуляции ---
#define PROBABILITY_CHANNEL_BUSY 0.75
#define PROBABILITY_COLLISION 0.25

// --- Глобальные переменные ---
extern std::atomic<int> g_collision_count;
extern std::atomic<bool> g_dynamic_info_enabled; // Флаг для отображения лога
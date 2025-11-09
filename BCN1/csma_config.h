#pragma once
#include <atomic>

// --- Управляющие символы для эмуляции CSMA/CD ---
#define ENQ 0x05 // Enquiry - Запрос на передачу (Carrier Sense)
#define ACK 0x06 // Acknowledge - Канал свободен (Clear to Send)
#define NAK 0x15 // Negative Acknowledge - Канал занят
#define COL 0x18 // Collision - Сигнал коллизии
#define JAM_SIGNAL 0xAA // Байт для jam-последовательности

// --- Параметры алгоритма CSMA/CD ---
#define MAX_ATTEMPTS 16      // Максимальное количество попыток передачи
#define SLOT_TIME_MS 50      // Время слота в миллисекундах (для расчета задержки)
#define JAM_SIGNAL_LENGTH 4  // Длина jam-последовательности в байтах
#define MAX_BACKOFF_DELAY_MS 2000 // Максимальная задержка 2 секунды

// --- Вероятности для эмуляции ---
#define PROBABILITY_CHANNEL_BUSY 0.75 // 75%
#define PROBABILITY_COLLISION 0.25    // 25%

// Глобальный счетчик коллизий для отображения в интерфейсе
extern std::atomic<int> g_collision_count;
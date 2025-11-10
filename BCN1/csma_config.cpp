#include "csma_config.h"

// Определение глобальных переменных
std::atomic<int> g_collision_count(0);
std::atomic<bool> g_dynamic_info_enabled(false); // По умолчанию подробный лог выключен
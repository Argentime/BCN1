#pragma once
#include <wtypes.h>
#include <string>
#include <vector>
#include "frame_config.h"

// Функция заменена на новую, с логикой CSMA/CD
bool send_with_csma_cd(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message,
    uint8_t& sequence, uint8_t address, uint8_t control, uint8_t variant);
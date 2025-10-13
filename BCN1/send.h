#pragma once
#include <wtypes.h>
#include <string>
#include <vector>

bool send_string_as_frame(FrameInfo& last_sent_frame, HANDLE hComm, const std::string& message,
    uint8_t& sequence,
    uint8_t address, 
    uint8_t control, 
    uint8_t variant);

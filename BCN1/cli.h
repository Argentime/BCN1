#pragma once
#include "frame_config.h"
#include <iostream>
#include <wtypes.h>

void print_frame_info(const FrameInfo& frame);
DWORD select_baud_rate();
void print_frame_info_with_hamming_status(const FrameInfo& frame);
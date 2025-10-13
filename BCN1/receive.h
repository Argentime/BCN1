#pragma once
#include <vector>
#include <random>
#include <wtypes.h>

bool split_info_into_payload_and_fcs(const std::vector<uint8_t>& info, std::vector<uint8_t>& payload_out, std::vector<uint8_t>& fcs_out);
void randomly_corrupt_payload_bits(std::vector<uint8_t>& payload);
bool receive_frame(HANDLE hComm);
#pragma once
#include <vector>
#include <random>
#include <wtypes.h>

void distort_encoded_payload(std::vector<uint8_t>& encoded_payload, std::mt19937& rng);
bool receive_frame(HANDLE hComm, std::mt19937& rng);
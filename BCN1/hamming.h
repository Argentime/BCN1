#pragma once
#include <vector>
int compute_hamming_m(size_t k_bits);
std::vector<uint8_t> build_fcs_from_payload(const std::vector<uint8_t>& payload);
std::vector<int> unpack_fcs_bits(const std::vector<uint8_t>& fcs_bytes, int expected_bits_count);
std::pair<uint64_t, int> compute_syndrome_and_parity(const std::vector<uint8_t>& payload, const std::vector<int>& received_fcs_bits);
std::pair<std::vector<uint8_t>, int> hamming_check_and_correct(std::vector<uint8_t> payload, const std::vector<uint8_t>& fcs_bytes);
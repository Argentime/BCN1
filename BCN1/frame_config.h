#pragma once
#include <vector>

#define FLAG 0x04

struct FrameInfo {
    uint8_t address = 0;
    uint8_t control = 0;
    uint8_t sequence = 0;
    uint8_t variant = 0;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> raw_frame; // с байт-стаффингом (то, что ушло по порту)
    std::vector<uint8_t> fcs_bytes; // сырые байты FCS
};

std::vector<uint8_t> build_information_field(uint8_t address,
	uint8_t control,
	uint8_t sequence,
	uint8_t variant,
	const std::vector<uint8_t>& payload);
std::vector<uint8_t> byte_stuff(const std::vector<uint8_t>& data);
std::vector<uint8_t> byte_unstuff(const std::vector<uint8_t>& data);
void parse_information_field(const std::vector<uint8_t>& info, FrameInfo& frame);
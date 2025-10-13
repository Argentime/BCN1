#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "main.h"
#include <algorithm> // для std::min
#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <locale.h>
#include <stdbool.h>
#include <limits>
#include <sstream>
#include <iomanip>
#include <random>
#include <cmath>
#include "io.h"

int compute_hamming_m(size_t k_bits) {
    int m = 1;
    // Для Hamming SECDED: нужно m такое, что 2^m >= k + m + 1
    // (мы потом добавим ещё общий паритет как отдельный бит)
    while ((1ULL << m) < (k_bits + m + 1ULL)) {
        ++m;
        if (m > 64) break;
    }
    return m;
}

// формирование FCS (m контрольных бит + 1 overall parity)
// Возвращает байтовый вектор (LSB-first) который содержит (m bits) then overall parity bit (в младших битах)
std::vector<uint8_t> build_fcs_from_payload(const std::vector<uint8_t>& payload) {
    // payload -> bits
    std::vector<int> data_bits;
    data_bits.reserve(payload.size() * 8);
    for (uint8_t b : payload) {
        for (int i = 0; i < 8; ++i) data_bits.push_back((b >> i) & 1); // LSB-first per byte
    }
    size_t k = data_bits.size();
    int m = compute_hamming_m(k);
    size_t n = k + m; // codeword length without overall parity
    // codeword positions 1..n (1-indexed); parity positions are powers of two
    std::vector<int> codeword(n + 1, 0); // index 0 unused

    // fill data bits into codeword positions that are not powers of two
    size_t di = 0;
    for (size_t pos = 1; pos <= n; ++pos) {
        bool is_parity_pos = ((pos & (pos - 1)) == 0);
        if (!is_parity_pos) {
            if (di < data_bits.size()) {
                codeword[pos] = data_bits[di++];
            }
            else {
                codeword[pos] = 0;
            }
        }
    }
    // compute parity bits
    std::vector<int> parity_bits(m + 1, 0); // 1..m
    for (int i = 1; i <= m; ++i) {
        int p = 0;
        size_t mask = 1u << (i - 1);
        for (size_t pos = 1; pos <= n; ++pos) {
            if (pos & mask) p ^= codeword[pos];
        }
        parity_bits[i] = p;
        // put in codeword for overall parity calc
        codeword[mask] = p;
    }
    // overall parity (parity of all bits in codeword including parity bits)
    int overall = 0;
    for (size_t pos = 1; pos <= n; ++pos) overall ^= codeword[pos];
    // prepare FCS bits: parity_bits[1..m] (in order) then overall
    std::vector<int> fcs_bits;
    for (int i = 1; i <= m; ++i) fcs_bits.push_back(parity_bits[i]);
    fcs_bits.push_back(overall); // last bit = overall parity
    // pack bits into bytes (LSB-first)
    size_t fcs_bits_count = fcs_bits.size();
    size_t fcs_bytes = (fcs_bits_count + 7) / 8;
    std::vector<uint8_t> fcs(fcs_bytes, 0);
    for (size_t i = 0; i < fcs_bits_count; ++i) {
        if (fcs_bits[i]) {
            size_t byte_idx = i / 8;
            size_t bit_idx = i % 8;
            fcs[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
    }
    return fcs;
}

// распаковка FCS байтов в битовый вектор (возвращает вектор<int> длины m+1)
std::vector<int> unpack_fcs_bits(const std::vector<uint8_t>& fcs_bytes, int expected_bits_count) {
    std::vector<int> bits;
    bits.reserve(expected_bits_count);
    for (int i = 0; i < expected_bits_count; ++i) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (byte_idx < (int)fcs_bytes.size()) {
            bits.push_back((fcs_bytes[byte_idx] >> bit_idx) & 1);
        }
        else bits.push_back(0);
    }
    return bits;
}

// Given payload bits and received fcs_bits (size m+1), compute syndrome and overall parity mismatch,
// return pair(syndrome (0 means none), overall_mismatch (0/1))
std::pair<uint64_t, int> compute_syndrome_and_parity(const std::vector<uint8_t>& payload, const std::vector<int>& received_fcs_bits) {
    // reconstruct codeword positions 1..n where n = k + m
    std::vector<int> data_bits;
    for (uint8_t b : payload) for (int i = 0; i < 8; ++i) data_bits.push_back((b >> i) & 1);
    size_t k = data_bits.size();
    int m = (int)received_fcs_bits.size() - 1; // last bit is overall
    size_t n = k + m;
    std::vector<int> codeword(n + 1, 0);
    // fill data bits into non-parity positions
    size_t di = 0;
    for (size_t pos = 1; pos <= n; ++pos) {
        bool is_parity_pos = ((pos & (pos - 1)) == 0);
        if (!is_parity_pos) {
            if (di < data_bits.size()) codeword[pos] = data_bits[di++];
            else codeword[pos] = 0;
        }
    }
    // place received parity bits into their positions
    for (int i = 1; i <= m; ++i) {
        size_t pos = (1u << (i - 1));
        if (pos <= n) codeword[pos] = received_fcs_bits[i - 1];
    }
    // compute syndrome: recompute parity bits and compare
    uint64_t syndrome = 0;
    for (int i = 1; i <= m; ++i) {
        int p = 0;
        size_t mask = 1u << (i - 1);
        for (size_t pos = 1; pos <= n; ++pos) {
            if (pos & mask) p ^= codeword[pos];
        }
        int received_p = received_fcs_bits[i - 1];
        if (p != received_p) syndrome |= (1ULL << (i - 1));
    }
    // compute overall parity over codeword
    int overall_calc = 0;
    for (size_t pos = 1; pos <= n; ++pos) overall_calc ^= codeword[pos];
    int received_overall = received_fcs_bits[m];
    int overall_mismatch = overall_calc ^ received_overall;
    return { syndrome, overall_mismatch };
}

// Apply correction: if single-bit error -> flip bit in payload or parity (if syndrome position is parity pos).
// Returns pair(corrected_payload_bytes, status): status 0=no error,1=corrected single,2=double detected/uncorrectable,3=parity-bit error fixed
std::pair<std::vector<uint8_t>, int> hamming_check_and_correct(std::vector<uint8_t> payload, const std::vector<uint8_t>& fcs_bytes) {
    // compute k,m and unpack fcs bits
    std::vector<int> data_bits;
    for (uint8_t b : payload) for (int i = 0; i < 8; ++i) data_bits.push_back((b >> i) & 1);
    size_t k = data_bits.size();
    int m = compute_hamming_m(k);
    int total_bits = m + 1;
    std::vector<int> received_fcs_bits = unpack_fcs_bits(fcs_bytes, total_bits);
    // DEBUG: print payload before correction
    std::cout << "[HAMMING] payload before: ";
    for (auto b : payload) std::cout << to_hex_string(b) << " ";
    std::cout << "\n[HAMMING] payload bits: ";
    for (auto b : payload) {
        for (int i = 0; i < 8; ++i) std::cout << ((b >> i) & 1);
        std::cout << " ";
    }
    std::cout << "\n[HAMMING] received FCS bits: ";
    for (int bit : received_fcs_bits) std::cout << bit;
    std::cout << std::endl;

    auto pr = compute_syndrome_and_parity(payload, received_fcs_bits);
    uint64_t syndrome = pr.first;
    int overall_mismatch = pr.second;
    std::cout << "[HAMMING] syndrome=" << syndrome << " overall_mismatch=" << overall_mismatch << std::endl;


    size_t n = k + m;
    if (syndrome == 0 && overall_mismatch == 0) {
        // no error
        return { payload, 0 };
    }
    else if (syndrome == 0 && overall_mismatch == 1) {
        // error in overall parity bit only -> can "fix" parity but data intact
        return { payload, 3 };
    }
    else if (syndrome != 0 && overall_mismatch == 1) {
        uint64_t pos = syndrome; // 1-indexed position of error within codeword (1..n)

        if (pos >= 1 && pos <= n) {
            // flip bit at pos (could be parity position or data position)
            bool is_parity = ((pos & (pos - 1)) == 0);
            if (is_parity) {
                // error in some parity bit -> flipping parity would be fixing FCS (we don't change payload)
                return { payload, 1 }; // treated as corrected single-bit (parity) — payload unchanged
            }
            else {
                // map pos to data bit index and flip it
                // find index (0..k-1) of data bit at position pos
                size_t di = 0;
                for (size_t p = 1; p <= n; ++p) {
                    bool is_par = ((p & (p - 1)) == 0);
                    if (!is_par) {
                        if (p == pos) {
                            // flip data bit at index di

                            size_t bit_idx = di;
                            size_t byte_idx = bit_idx / 8;
                            size_t bit_in_byte = bit_idx % 8;
                            std::cout << "[HAMMING] will flip codeword pos " << pos;
                            if (!is_parity) std::cout << " -> data bit index " << di << " (byte " << byte_idx << " bit " << bit_in_byte << ")";
                            std::cout << std::endl;
                            if (byte_idx < payload.size()) {
                                payload[byte_idx] ^= (uint8_t)(1u << bit_in_byte);
                                std::cout << "[HAMMING] payload after: ";
                                for (auto b : payload) std::cout << to_hex_string(b) << " ";
                                std::cout << std::endl;
                                return { payload, 1 };
                            }

                            else {
                                return { payload, 2 };
                            }
                        }
                        di++;
                    }
                }
                return { payload, 2 };
            }
        }
        else {
            return { payload, 2 };
        }
    }
    else if (syndrome != 0 && overall_mismatch == 0) {
        // detected double-bit error (syndrome nonzero but overall parity says even) -> uncorrectable
        return { payload, 2 };
    }
    else {
        return { payload, 2 };
    }
}
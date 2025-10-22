/*
 *  Copyright 2018-2024 Vladimir Tolmachev
 *
 *   Author: Vladimir Tolmachev
 *  Project: Dungeon: Age of Heroes
 *   e-mail: tolm_vl@hotmail.com
 *
 * If you received the code is not the author, please contact me
 */

#include "uuid4.h"
#include <random>
#include <sstream>

namespace uuid4 {
static std::random_device rd;
static std::mt19937_64 gen(rd());
static std::uniform_int_distribution<> dis(0, 15);
static std::uniform_int_distribution<> dis2(8, 11);

std::string generate() {
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}
} // namespace uuid4

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

bool mbotWebStart(uint16_t port = 80);
void mbotWebStop();
bool mbotWebIsActive();
void mbotWebAvatarTick();

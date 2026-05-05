/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "avatar.h"

#include <lvgl.h>
#include <memory>
#include <string>
#include <string_view>

namespace stackchan::avatar {

inline constexpr const char* kAvatarSkinDefault  = "default";
inline constexpr const char* kAvatarSkinRoboEyes = "roboeyes";

std::string getConfiguredSkinName();
void setConfiguredSkinName(std::string_view skin);
const char* getSkinDisplayName(std::string_view skin);
std::unique_ptr<Avatar> createAvatarForSkin(std::string_view skin, lv_obj_t* parent,
                                            const lv_font_t* font = &lv_font_montserrat_16);
std::unique_ptr<Avatar> createConfiguredAvatar(lv_obj_t* parent, const lv_font_t* font = &lv_font_montserrat_16);

}  // namespace stackchan::avatar

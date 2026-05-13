/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../avatar/avatar.h"
#include <lvgl.h>
#include <memory>

namespace stackchan::avatar {

/**
 * Build avatar from persistent settings NS "display", key "avatar_skin"
 * Values: "default" | "roboeyes".
 */
std::unique_ptr<Avatar> make_avatar_from_settings(lv_obj_t* parent,
                                                    const lv_font_t* font = nullptr);

}  // namespace stackchan::avatar

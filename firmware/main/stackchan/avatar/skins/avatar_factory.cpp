/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "avatar_factory.h"
#include "default/default.h"
#include "roboeyes/roboeyes.h"
#include <lvgl.h>
#include <memory>
#include <settings.h>
#include <string>

namespace stackchan::avatar {

std::unique_ptr<Avatar> make_avatar_from_settings(lv_obj_t* parent,
                                                    const lv_font_t* font)
{
    Settings       settings("display",
                            false);
    const std::string skin =
        settings.GetString(
            "avatar_skin",
            "default");

    if (skin == "roboeyes") {
        auto avatar =
            std::make_unique<RoboEyesAvatar>();
        avatar->init(parent,
                     font ?
                         font :
                         &lv_font_montserrat_16);
        return avatar;
    }

    auto avatar =
        std::make_unique<DefaultAvatar>();
    avatar->init(parent,
                 font ?
                     font :
                     &lv_font_montserrat_16);
    return avatar;
}

}  // namespace stackchan::avatar
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "avatar_factory.h"

#include <settings.h>

namespace stackchan::avatar {
namespace {

constexpr const char* kAvatarSettingsNamespace = "avatar";
constexpr const char* kAvatarSkinSettingsKey   = "skin";

bool isKnownSkin(std::string_view skin)
{
    return skin == kAvatarSkinDefault || skin == kAvatarSkinRoboEyes;
}

}  // namespace

std::string getConfiguredSkinName()
{
    Settings settings(kAvatarSettingsNamespace, false);
    auto skin = settings.GetString(kAvatarSkinSettingsKey, kAvatarSkinDefault);
    return isKnownSkin(skin) ? skin : kAvatarSkinDefault;
}

void setConfiguredSkinName(std::string_view skin)
{
    Settings settings(kAvatarSettingsNamespace, true);
    auto selected_skin = isKnownSkin(skin) ? skin : std::string_view(kAvatarSkinDefault);
    settings.SetString(kAvatarSkinSettingsKey, std::string(selected_skin));
}

const char* getSkinDisplayName(std::string_view skin)
{
    if (skin == kAvatarSkinRoboEyes) {
        return "RoboEyes";
    }
    return "Default";
}

std::unique_ptr<Avatar> createAvatarForSkin(std::string_view skin, lv_obj_t* parent, const lv_font_t* font)
{
    if (skin == kAvatarSkinRoboEyes) {
        auto avatar = std::make_unique<RoboEyesAvatar>();
        avatar->init(parent, font);
        return avatar;
    }

    auto avatar = std::make_unique<DefaultAvatar>();
    avatar->init(parent, font);
    return avatar;
}

std::unique_ptr<Avatar> createConfiguredAvatar(lv_obj_t* parent, const lv_font_t* font)
{
    return createAvatarForSkin(getConfiguredSkinName(), parent, font);
}

}  // namespace stackchan::avatar

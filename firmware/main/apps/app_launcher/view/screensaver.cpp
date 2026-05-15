/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake_log.h>
#include <fmt/format.h>

using namespace view;
using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace uitk::games;
using namespace uitk::games::dvd_screensaver;

static const Vector2 _screen_size               = {320, 240};
static const int _logo_id                       = 666;
static const std::vector<uint32_t> _logo_colors = {
    0xffffff, 0xfffa01, 0xff8300, 0x00feff, 0xff2600, 0xbe00ff, 0x0026ff, 0xff008b,
};
static const uint32_t _bg_color = 0x000000;

namespace {

constexpr std::string_view kDefault_format = "24h_hm";

static std::pair<std::string, std::string> format_clock_lines(const tm& local_tm, std::string_view key)
{
    // Date line ("Fri 15 May"); only returned for keys that carry a second row.
    if (key == "24h_hm_date") {
        static const char* const kWd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char* const kMo[]   = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::string t = fmt::format("{:02d}:{:02d}", local_tm.tm_hour, local_tm.tm_min);
        std::string d =
            fmt::format("{} {} {}", kWd[local_tm.tm_wday], local_tm.tm_mday, kMo[local_tm.tm_mon]);
        return {t, d};
    }

    std::string t;
    if (key == "12h_hm_ampm") {
        int hour12 = local_tm.tm_hour % 12;
        if (hour12 == 0) {
            hour12 = 12;
        }
        t = fmt::format("{}:{:02d} {}", hour12, local_tm.tm_min, local_tm.tm_hour >= 12 ? "PM" : "AM");
    } else if (key == "24h_hms") {
        t = fmt::format("{:02d}:{:02d}:{:02d}", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
    } else {
        // 24h_hm and unknown formats
        if (key != "24h_hm" && key != kDefault_format) {
            mclog::tagWarn("Screensaver", "unknown clock_format '{}', fallback to {}", key, kDefault_format);
        }
        t = fmt::format("{:02d}:{:02d}", local_tm.tm_hour, local_tm.tm_min);
    }

    return {t, {}};
}

}  // namespace

Screensaver::~Screensaver()
{
    _prev_screen->load();
}

void Screensaver::applyLabelColors()
{
    lv_color_t c = lv_color_hex(_logo_colors[static_cast<size_t>(_color_index) % _logo_colors.size()]);
    _time_label->setTextColor(c);
    if (_date_label) {
        _date_label->setTextColor(c);
    }
}

void Screensaver::refreshClockUi()
{
    auto now   = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);

    tm local_tm{};
    localtime_r(&now_t, &local_tm);

    auto lines = format_clock_lines(local_tm, _clock_format_key);
    _time_label->setText(lines.first);
    if (_date_label) {
        _date_label->setText(lines.second);
    }
}

void Screensaver::onInit()
{
    _clock_format_key = GetHAL().getClockFormat();
    if (_clock_format_key.empty()) {
        _clock_format_key = std::string(kDefault_format);
    }

    const bool with_date = (_clock_format_key == "24h_hm_date");
    _logo_width            = 250;
    _logo_height           = with_date ? 110 : 70;

    _prev_screen = std::make_unique<ScreenActive>();

    _screen = std::make_unique<Screen>();
    _screen->setBgColor(lv_color_hex(_bg_color));
    _screen->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _screen->setPadding(0, 0, 0, 0);
    _screen->load();

    _logo = std::make_unique<Container>(_screen->get());
    _logo->setSize(_logo_width, _logo_height);
    _logo->setBgOpa(LV_OPA_TRANSP);
    _logo->align(LV_ALIGN_TOP_LEFT, 2333, 2333);
    _logo->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _logo->setPadding(0, 0, 0, 0);
    _logo->setBorderWidth(0);
    _logo->setRadius(0);

    _time_label = std::make_unique<Label>(_logo->get());
    _time_label->setText("");
    _time_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _time_label->setWidth(_logo_width);
    _time_label->setTextFont(&lv_font_montserrat_48);
    applyLabelColors();
    _time_label->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _time_label->align(LV_ALIGN_CENTER, 0, with_date ? -16 : 0);

    if (with_date) {
        _date_label = std::make_unique<Label>(_logo->get());
        _date_label->setText("");
        _date_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        _date_label->setWidth(_logo_width);
        _date_label->setTextFont(&lv_font_montserrat_16);
        applyLabelColors();
        _date_label->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
        _date_label->align(LV_ALIGN_CENTER, 0, 42);
    }

    refreshClockUi();
    _last_clock_ms = GetHAL().millis();
}

void Screensaver::onBuildLevel()
{
    addScreenFrameAsWall(_screen_size);

    auto& random      = Random::getInstance();
    Vector2 direction = {random.getFloat(0.3f, 0.7f), random.getFloat(0.3f, 0.7f)};
    direction         = direction.normalized();

    const Vector2 logo_size = {_logo_width, _logo_height};
    addLogo(_logo_id, {_screen_size.width / 2, _screen_size.height / 2}, logo_size, direction, 110);
}

void Screensaver::onRender(float dt)
{
    uint32_t now_ms = GetHAL().millis();
    if (now_ms - _last_clock_ms >= 1000) {
        _last_clock_ms = now_ms;
        refreshClockUi();
    }

    getWorld().forEachObject([&](GameObject* obj) {
        if (obj->groupId == _logo_id) {
            auto p = obj->get<Transform>()->position;
            _logo->setPos((int)std::lround(p.x) - _logo_width / 2, (int)std::lround(p.y) - _logo_height / 2);
        }
    });
}

void Screensaver::onLogoCollide(int logoGroupId)
{
    _color_index++;
    if (_color_index >= static_cast<int>(_logo_colors.size())) {
        _color_index = 0;
    }

    applyLabelColors();
}

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>
#include <cstdint>
#include <functional>
#include <smooth_lvgl.hpp>
#include <string_view>
#include <uitk/short_namespace.hpp>

namespace view {

class LoadingPage {
public:
    LoadingPage(uint32_t bgColor = 0x000000, uint32_t textColor = 0xFFFFFF)
    {
        _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _panel->setBgColor(lv_color_hex(bgColor));
        _panel->align(LV_ALIGN_CENTER, 0, 0);
        _panel->setSize(320, 240);
        _panel->setBorderWidth(0);
        _panel->setRadius(0);

        _msg = std::make_unique<uitk::lvgl_cpp::Label>(_panel->get());
        _msg->setTextFont(&lv_font_montserrat_20);
        _msg->setTextColor(lv_color_hex(textColor));
        _msg->setTextAlign(LV_TEXT_ALIGN_CENTER);
        _msg->align(LV_ALIGN_CENTER, 0, 0);
        _msg->setText("");
        _msg->setWidth(300);
    }

    void setMessage(std::string_view msg)
    {
        _msg->setText(msg);
    }

    /** LV_EVENT_LONG_PRESSED on full-screen panel (for debug actions). */
    void setOnLongPress(std::function<void()> cb);

private:
    static void onLongPressThunk(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _msg;
    std::function<void()> _long_press_cb;
};

inline void LoadingPage::onLongPressThunk(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    auto* self = static_cast<LoadingPage*>(lv_event_get_user_data(e));
    if (self && self->_long_press_cb) {
        self->_long_press_cb();
    }
}

inline void LoadingPage::setOnLongPress(std::function<void()> cb)
{
    _long_press_cb = std::move(cb);
    lv_obj_add_flag(_panel->get(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_panel->get(), onLongPressThunk, LV_EVENT_LONG_PRESSED, this);
}

}  // namespace view

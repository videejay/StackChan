/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../modifiable.h"
#include <cstdint>

namespace stackchan {

class BodyMotionModifier : public Modifier {
public:
    static constexpr const char* kName = "body_motion";
    const char* name() const override
    {
        return kName;
    }

    void _update(Modifiable& stackchan) override;

private:
    uint32_t next_update_ms_ = 0;
    uint32_t next_too_close_feedback_ms_ = 0;
    bool turn_left_next_     = true;
    bool too_close_active_   = false;
};

}  // namespace stackchan

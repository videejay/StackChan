/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace stackchan::diag {

/** Starts a background task (Core 1) that logs heap stats every 30 s. */
void start_heap_stats_logger();

}  // namespace stackchan::diag

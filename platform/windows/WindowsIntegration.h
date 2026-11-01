// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace aw {
void recoverDefaultAudio();
bool switchDefaultAudio(const std::wstring& output, const std::wstring& input, bool restore,
                        std::string& error);
} // namespace aw

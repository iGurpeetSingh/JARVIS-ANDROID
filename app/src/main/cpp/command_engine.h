#pragma once

#include <string>

namespace jarvis {
enum class Command {
    Unknown = 0,
    VolumeUp,
    VolumeDown,
    OpenWhatsApp,
    OpenYouTube,
    OpenDialer
};

Command classify_command(const std::string& raw);
}

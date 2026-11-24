#pragma once

#include <string>

namespace jarvis {
enum class Command {
    Unknown = 0,
    VolumeUp,
    VolumeDown,
    OpenWhatsApp,
    OpenWhatsAppContact,
    OpenYouTube,
    OpenDialer
};

Command classify_command(const std::string& raw);
std::string extract_contact_query(const std::string& raw);
}

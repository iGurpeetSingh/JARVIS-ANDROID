#include "command_engine.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using jarvis::Command;

    assert(jarvis::classify_command("volume up") == Command::VolumeUp);
    assert(jarvis::classify_command("आवाज़ बढ़ा दो") == Command::VolumeUp);
    assert(jarvis::classify_command("volume kam karo") == Command::VolumeDown);
    assert(jarvis::classify_command("व्हाट्सएप खोलो") == Command::OpenWhatsApp);
    assert(jarvis::classify_command("youtube kholo") == Command::OpenYouTube);
    assert(jarvis::classify_command("फोन लगाओ") == Command::OpenDialer);

    const std::string english = jarvis::extract_contact_query("search for Pradeep");
    assert(english == "Pradeep");
    assert(jarvis::classify_command("search for Pradeep") == Command::OpenWhatsAppContact);

    const std::string hindi = jarvis::extract_contact_query("कॉन्टैक्ट खोजो प्रवीण कुमार");
    assert(hindi == "प्रवीण कुमार");
    assert(jarvis::classify_command("कॉन्टैक्ट खोजो प्रवीण कुमार") == Command::OpenWhatsAppContact);

    const std::string chat = jarvis::extract_contact_query("open chat with Sumit Kumar");
    assert(chat == "Sumit Kumar");

    const std::string message = jarvis::extract_contact_query("send message to Nofil");
    assert(message == "Nofil");

    assert(jarvis::classify_command("what is the weather") == Command::Unknown);

    std::cout << "JARVIS native command engine tests passed.\n";
    return 0;
}

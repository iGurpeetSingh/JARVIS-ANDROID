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

    const std::string youtubeQuery = jarvis::extract_youtube_query("search youtube for Punjabi songs");
    assert(youtubeQuery == "Punjabi songs");
    assert(jarvis::classify_command("search youtube for Punjabi songs") == Command::SearchYouTube);

    const std::string hindiYoutubeQuery = jarvis::extract_youtube_query("यूट्यूब पर खोजो पुराने हिंदी गाने");
    assert(hindiYoutubeQuery == "पुराने हिंदी गाने");

    const std::string requestedContact = jarvis::extract_contact_query("send message to Nofil saying I am on my way");
    const std::string requestedMessage = jarvis::extract_contact_message("send message to Nofil saying I am on my way");
    assert(requestedContact == "Nofil");
    assert(requestedMessage == "I am on my way");

    const std::string hindiContact = jarvis::extract_contact_query("Nofil ko WhatsApp par message bhejo, kehna main aa raha hoon");
    const std::string hindiMessage = jarvis::extract_contact_message("Nofil ko WhatsApp par message bhejo, kehna main aa raha hoon");
    assert(hindiContact == "Nofil");
    assert(hindiMessage == "main aa raha hoon");

    assert(jarvis::classify_command("haan") == Command::ConfirmPendingAction);
    assert(jarvis::classify_command("नहीं") == Command::CancelPendingAction);

    const std::string chat = jarvis::extract_contact_query("open chat with Sumit Kumar");
    assert(chat == "Sumit Kumar");

    const std::string message = jarvis::extract_contact_query("send message to Nofil");
    assert(message == "Nofil");

    assert(jarvis::classify_command("what is the weather") == Command::Unknown);

    std::cout << "JARVIS native command engine tests passed.\n";
    return 0;
}

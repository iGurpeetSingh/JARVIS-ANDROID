#include "command_engine.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

namespace {
std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return ch < 0x80 ? static_cast<char>(std::tolower(ch))
                                        : static_cast<char>(ch);
                   });
    return value;
}

bool contains_any(const std::string& text,
                  const std::initializer_list<const char*>& phrases) {
    for (const char* phrase : phrases) {
        if (text.find(phrase) != std::string::npos) return true;
    }
    return false;
}
}


namespace jarvis {
std::string extract_contact_query(const std::string& raw) {
    const std::string text = lower_ascii(raw);
    const std::initializer_list<const char*> markers = {
        "search for ", "find contact ", "look up contact ",
        "open chat with ", "message to ", "send message to ",
        "संपर्क खोजो ", "कॉन्टैक्ट खोजो ", "कॉन्टैक्ट ढूंढो ",
        "व्हाट्सएप पर खोजो ", "व्हाट्सऐप पर खोजो "
    };
    size_t start = std::string::npos;
    size_t markerLength = 0;
    for (const char* marker : markers) {
        const size_t pos = text.find(marker);
        if (pos != std::string::npos && (start == std::string::npos || pos < start)) {
            start = pos;
            markerLength = std::char_traits<char>::length(marker);
        }
    }

    std::string candidate;
    if (start != std::string::npos) {
        candidate = raw.substr(start + markerLength);
    } else {
        const size_t englishEnd = text.find(" ko whatsapp");
        const size_t hindiEnd = text.find(" को व्हाट्स");
        size_t end = std::min(englishEnd, hindiEnd);
        if (end == std::string::npos) end = englishEnd == std::string::npos ? hindiEnd : englishEnd;
        if (end != std::string::npos && end > 0) {
            candidate = raw.substr(0, end);
            const size_t whatsapp = lower_ascii(candidate).rfind("whatsapp");
            if (whatsapp != std::string::npos) candidate.erase(whatsapp);
        }
    }
    if (candidate.empty()) return {};

    const std::string lowerCandidate = lower_ascii(candidate);
    const std::initializer_list<const char*> endings = {
        " saying ", " with message ", " message: ", " text: ",
        " on whatsapp", " whatsapp", " please", " ko kholo",
        " को खोलो", " को खोल", " को व्हाट्स", " पर खोलो",
        " पर खोल", " open whatsapp", " कहना ", " बोलो ",
        " संदेश ", " मैसेज ", " लिखो "
    };
    size_t trimAt = candidate.size();
    for (const char* ending : endings) {
        const size_t pos = lowerCandidate.find(ending);
        if (pos != std::string::npos) trimAt = std::min(trimAt, pos);
    }
    candidate.resize(trimAt);
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) || ch == ',' || ch == '.'; };
    while (!candidate.empty() && isSpace(static_cast<unsigned char>(candidate.front()))) candidate.erase(candidate.begin());
    while (!candidate.empty() && isSpace(static_cast<unsigned char>(candidate.back()))) candidate.pop_back();
    if (candidate.size() > 64) candidate.resize(64);
    return candidate;
}

std::string extract_contact_message(const std::string& raw) {
    const std::string text = lower_ascii(raw);
    const std::initializer_list<const char*> markers = {
        " saying ", " with message ", " message: ", " text: ", " kehna ", " bolna ", " bol do ", " tell him ",
        " कहना ", " बोलो ", " बोलना ", " संदेश ", " मैसेज ", " लिखो "
    };
    size_t start = std::string::npos;
    size_t markerLength = 0;
    for (const char* marker : markers) {
        const size_t pos = text.find(marker);
        if (pos != std::string::npos && (start == std::string::npos || pos < start)) {
            start = pos;
            markerLength = std::char_traits<char>::length(marker);
        }
    }
    if (start == std::string::npos) return {};
    std::string message = raw.substr(start + markerLength);
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front()))) message.erase(message.begin());
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.back()))) message.pop_back();
    if (message.size() > 1000) message.resize(1000);
    return message;
}

std::string extract_youtube_query(const std::string& raw) {
    const std::string text = lower_ascii(raw);
    const std::initializer_list<const char*> markers = {
        "search youtube for ", "search on youtube for ",
        "youtube search for ", "youtube par search karo ",
        "यूट्यूब पर खोजो ", "यूट्यूब पर सर्च करो ",
        "यूट्यूब में खोजो ", "यूट्यूब पर ढूंढो "
    };
    size_t start = std::string::npos;
    size_t length = 0;
    for (const char* marker : markers) {
        const size_t pos = text.find(marker);
        if (pos != std::string::npos && (start == std::string::npos || pos < start)) {
            start = pos;
            length = std::char_traits<char>::length(marker);
        }
    }
    if (start == std::string::npos) return {};
    std::string query = raw.substr(start + length);
    const std::string lowerQuery = lower_ascii(query);
    for (const char* ending : {" on youtube", " in youtube", " please", " प्लीज"}) {
        const size_t pos = lowerQuery.find(ending);
        if (pos != std::string::npos) query.resize(std::min(query.size(), pos));
    }
    while (!query.empty() && std::isspace(static_cast<unsigned char>(query.back()))) query.pop_back();
    while (!query.empty() && std::isspace(static_cast<unsigned char>(query.front()))) query.erase(query.begin());
    if (query.size() > 160) query.resize(160);
    return query;
}

Command classify_command(const std::string& raw) {
    const std::string text = lower_ascii(raw);
    if (!extract_contact_query(raw).empty()) return Command::OpenWhatsAppContact;
    if (!extract_youtube_query(raw).empty()) return Command::SearchYouTube;
    if (contains_any(text, {"yes", "haan", "han", "confirm", "send it", "haan bhejo", "han bhejo", "हाँ", "हां", "हाँ भेजो", "हां भेजो", "ठीक है भेजो"})) return Command::ConfirmPendingAction;
    if (contains_any(text, {"no", "nahi", "nahin", "cancel", "mat bhejo", "मत भेजो", "नहीं", "नही", "नहीं भेजना", "रद्द करो"})) return Command::CancelPendingAction;

    if (contains_any(text, {"volume up", "increase volume", "turn up volume", "volume badhao",
                            "awaz badhao", "आवाज बढ़", "वॉल्यूम बढ़", "आवाज़ बढ़",
                            "आवाज तेज", "आवाज़ तेज", "आवाज़ ऊँची"})) {
        return Command::VolumeUp;
    }
    if (contains_any(text, {"volume down", "decrease volume", "turn down volume", "volume kam karo",
                            "awaz kam", "आवाज कम", "वॉल्यूम कम", "आवाज़ कम",
                            "आवाज धीमी", "आवाज़ धीमी"})) {
        return Command::VolumeDown;
    }
    if (contains_any(text, {"whatsapp", "व्हाट्सएप", "व्हाट्स ऐप", "व्हाट्सएप खोल",
                            "व्हाट्सऐप खोल", "whatsapp kholo", "whatsapp open"})) {
        return Command::OpenWhatsApp;
    }
    if (contains_any(text, {"youtube", "यूट्यूब", "youtube kholo", "youtube open", "यूट्यूब खोल"})) {
        return Command::OpenYouTube;
    }
    if (contains_any(text, {"call", "कॉल", "फोन लगाओ", "डायलर खोल", "phone kholo",
                            "open dialer", "dialer kholo"})) {
        return Command::OpenDialer;
    }
    return Command::Unknown;
}
}

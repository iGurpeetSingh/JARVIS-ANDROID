#include "contact-engine.h"

#include <algorithm>
#include <cctype>

namespace {
std::string normalize_name(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return ch < 0x80 ? static_cast<char>(std::tolower(ch))
                             : static_cast<char>(ch);
        });
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

void append_unique(std::vector<jarvis::ContactCandidate>& target,
                   const jarvis::ContactCandidate& candidate) {
    const std::string normalized = jarvis::normalize_phone_number(candidate.phone);
    const auto found = std::find_if(target.begin(), target.end(), [&](const auto& existing) {
        return jarvis::normalize_phone_number(existing.phone) == normalized;
    });
    if (found == target.end()) target.push_back(candidate);
}
}

namespace jarvis {

std::string normalize_phone_number(const std::string& value) {
    std::string digits;
    digits.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isdigit(ch)) digits.push_back(static_cast<char>(ch));
    }
    if (digits.rfind("00", 0) == 0) digits.erase(0, 2);
    if (digits.size() == 11 && digits.front() == '0') digits.erase(digits.begin());
    if (digits.size() == 10) digits = "91" + digits;
    return digits;
}

ContactLookupResult find_contact_matches(
    const std::string& query, const std::vector<ContactCandidate>& contacts) {
    ContactLookupResult result;
    const std::string needle = normalize_name(query);
    if (needle.empty()) return result;

    std::vector<ContactCandidate> exact;
    std::vector<ContactCandidate> partial;
    for (const auto& contact : contacts) {
        const std::string name = normalize_name(contact.name);
        const std::string phone = normalize_phone_number(contact.phone);
        if (name.empty() || phone.size() < 10) continue;
        ContactCandidate normalized{contact.name, phone};
        if (name == needle) append_unique(exact, normalized);
        else if (name.find(needle) != std::string::npos) append_unique(partial, normalized);
    }

    result.matches = exact.empty() ? std::move(partial) : std::move(exact);
    if (result.matches.empty()) {
        result.status = ContactLookupStatus::NotFound;
    } else if (result.matches.size() == 1) {
        result.status = ContactLookupStatus::Unique;
        result.selected = result.matches.front();
    } else {
        result.status = ContactLookupStatus::Ambiguous;
    }
    return result;
}

} // namespace jarvis

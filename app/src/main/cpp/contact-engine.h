#pragma once

#include <string>
#include <vector>

namespace jarvis {

struct ContactCandidate {
    std::string name;
    std::string phone;
};

enum class ContactLookupStatus {
    NotFound = 0,
    Unique,
    Ambiguous
};

struct ContactLookupResult {
    ContactLookupStatus status = ContactLookupStatus::NotFound;
    ContactCandidate selected;
    std::vector<ContactCandidate> matches;
};

std::string normalize_phone_number(const std::string& value);
ContactLookupResult find_contact_matches(
    const std::string& query, const std::vector<ContactCandidate>& contacts);

} // namespace jarvis

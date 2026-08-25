#include "deidentifier.hpp"

#include <algorithm>
#include <cctype>

namespace deid {

namespace {

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::string entityTypeToString(EntityType type) {
    switch (type) {
    case EntityType::Email: return "EMAIL";
    case EntityType::Phone: return "PHONE";
    case EntityType::IPAddress: return "IP";
    case EntityType::IDNumber: return "ID";
    case EntityType::Date: return "DATE";
    case EntityType::URLToken: return "TOKEN";
    case EntityType::Person: return "PERSON";
    case EntityType::Location: return "LOCATION";
    case EntityType::Organization: return "ORG";
    }
    return "ENTITY";
}

} // namespace

DeidentificationResult Deidentifier::deidentifyText(const std::string& text) {
    const auto rawEntities = detector_.detectAll(text);
    const auto resolvedEntities = resolveOverlaps(rawEntities);

    DeidentificationResult result;
    std::size_t previousEnd = 0;

    for (const auto& entity : resolvedEntities) {
        result.text.append(text, previousEnd, entity.start - previousEnd);
        result.text += getPseudonym(entity.text, entity.type);
        previousEnd = entity.start + entity.length;
        ++result.redactions;
    }

    result.text.append(text, previousEnd, std::string::npos);
    return result;
}

std::vector<DetectedEntity> Deidentifier::resolveOverlaps(std::vector<DetectedEntity> entities) const {
    // Sort by position, then by highest score, then by longest match length
    std::sort(entities.begin(), entities.end(), [](const DetectedEntity& a, const DetectedEntity& b) {
        if (a.start != b.start) return a.start < b.start;
        if (a.score != b.score) return a.score > b.score;
        return a.length > b.length;
    });

    std::vector<DetectedEntity> resolved;
    std::size_t lastEnd = 0;

    for (const auto& entity : entities) {
        if (entity.start >= lastEnd) {
            resolved.push_back(entity);
            lastEnd = entity.start + entity.length;
        }
    }

    return resolved;
}

std::string Deidentifier::getPseudonym(const std::string& entityText, EntityType type) {
    const std::string key = toLower(entityText);
    const auto existing = pseudonyms_.find(key);
    if (existing != pseudonyms_.end()) {
        return existing->second;
    }

    const std::string prefix = entityTypeToString(type);
    const std::string pseudonym = "[" + prefix + "_" + std::to_string(pseudonyms_.size() + 1) + "]";
    pseudonyms_[key] = pseudonym;
    return pseudonym;
}

} // namespace deid

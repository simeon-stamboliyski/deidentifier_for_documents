#include "detectors.hpp"
#include <algorithm>
#include <cctype>

namespace deid {

EntityDetector::EntityDetector() {
    // Regex detection setup
    regexes_ = {
        {EntityType::Email, std::regex(R"(([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}))")},
        {EntityType::Phone, std::regex(R"(\b(\+\d{1,3}[- ]?)?\(?\d{3}\)?[- ]?\d{3}[- ]?\d{4}\b)")},
        {EntityType::IPAddress, std::regex(R"(\b(?:[0-9]{1,3}\.){3}[0-9]{1,3}\b)")},
        {EntityType::IDNumber, std::regex(R"(\b\d{3}-\d{2}-\d{4}\b|\bID:\s*[A-Z0-9]{6,10}\b)")},
        {EntityType::Date, std::regex(R"(\b\d{1,2}[/-]\d{1,2}[/-]\d{2,4}\b|\b(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]* \d{1,2},? \d{4}\b)")},
        {EntityType::URLToken, std::regex(R"(https?://[^\s/$.?#].[^\s]*|\btoken=[A-Za-z0-9%_-]+\b)")}
    };

    // Dictionary lookup setup
    dictionaries_ = {
        {EntityType::Location, {"New York", "London", "Tokyo", "Paris", "Chicago", "Boston"}},
        {EntityType::Organization, {"General Hospital", "St. Jude Clinic", "Acme Corp", "FBI", "IRS", "United Health"}}
    };
}

std::vector<DetectedEntity> EntityDetector::detectAll(const std::string& text) const {
    std::vector<DetectedEntity> results;
    detectRegex(text, results);
    detectDictionaries(text, results);
    detectNER(text, results);
    return results;
}

void EntityDetector::detectRegex(const std::string& text, std::vector<DetectedEntity>& results) const {
    for (const auto& [type, pattern] : regexes_) {
        for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
            results.push_back({
                type,
                static_cast<std::size_t>(it->position()),
                static_cast<std::size_t>(it->length()),
                it->str(),
                0.95 // High confidence for exact regex match
            });
        }
    }
}

void EntityDetector::detectDictionaries(const std::string& text, std::vector<DetectedEntity>& results) const {
    for (const auto& [type, terms] : dictionaries_) {
        for (const auto& term : terms) {
            std::size_t pos = text.find(term);
            while (pos != std::string::npos) {
                results.push_back({
                    type,
                    pos,
                    term.length(),
                    term,
                    0.85 // High confidence for exact dictionary match
                });
                pos = text.find(term, pos + term.length());
            }
        }
    }
}

void EntityDetector::detectNER(const std::string& text, std::vector<DetectedEntity>& results) const {
    // Heuristic NER for multi-word capitalized names (e.g., "John Smith", "Dr. Jane Doe")
    static const std::regex namePattern(R"(\b(?:Dr\.|Mr\.|Ms\.|Mrs\.)?\s*[A-Z][a-z]+\s+[A-Z][a-z]+\b)");
    for (std::sregex_iterator it(text.begin(), text.end(), namePattern), end; it != end; ++it) {
        results.push_back({
            EntityType::Person,
            static_cast<std::size_t>(it->position()),
            static_cast<std::size_t>(it->length()),
            it->str(),
            0.70 // Contextual heuristic confidence
        });
    }
}

} // namespace deid

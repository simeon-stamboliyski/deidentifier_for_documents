#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "detectors.hpp"

namespace deid {

struct DeidentificationResult {
    std::string text;
    std::size_t redactions = 0;
};

class Deidentifier {
public:
    DeidentificationResult deidentifyText(const std::string& text);

private:
    std::vector<DetectedEntity> resolveOverlaps(std::vector<DetectedEntity> entities) const;
    std::string getPseudonym(const std::string& entityText, EntityType type);

    EntityDetector detector_;
    std::unordered_map<std::string, std::string> pseudonyms_;
};

} // namespace deid

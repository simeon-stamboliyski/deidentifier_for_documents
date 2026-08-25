#pragma once

#include <filesystem>
#include <string>

namespace deid {

class MetadataProcessor {
public:
    static std::string stripMetadata(const std::string& text, const std::filesystem::path& path);
};

} // namespace deid

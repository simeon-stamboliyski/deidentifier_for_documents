#include "metadata.hpp"
#include <regex>

namespace deid {

std::string MetadataProcessor::stripMetadata(const std::string& text, const std::filesystem::path& path) {
    std::string result = text;
    const std::string ext = path.extension().string();

    if (ext == ".md" || ext == ".txt") {
        // Strip YAML frontmatter metadata (e.g., Author, Date, Revision header)
        static const std::regex yamlFrontmatter(R"(^---\s*[\s\S]*?---\s*)");
        result = std::regex_replace(result, yamlFrontmatter, "");

        // Strip HTML-style document comments and tracked revision tags
        static const std::regex htmlComments(R"(<!--[\s\S]*?-->)");
        result = std::regex_replace(result, htmlComments, "");

        // Strip explicit author/metadata headers
        static const std::regex metadataLines(R"(?i)^(Author|Created|Revision|Owner):\s*.*$\n?)");
        result = std::regex_replace(result, metadataLines, "");
    }

    return result;
}

} // namespace deid

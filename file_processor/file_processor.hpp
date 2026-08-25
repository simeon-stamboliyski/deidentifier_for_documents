#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>


namespace deid {

class Deidentifier;
namespace fs = std::filesystem;

enum class FileStatus { Processed, Skipped, Failed };

struct FileResult {
    FileStatus status;
    std::size_t redactions = 0;
    std::string message;
};

struct ProcessingSummary {
    std::size_t processed = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
    std::size_t redactions = 0;
};

fs::path getInputFolder(int argc, char* argv[]);
fs::path createOutputFolder(const fs::path& inputFolder);
std::vector<fs::path> findSupportedFiles(const fs::path& inputFolder);

FileResult processFile(
    const fs::path& inputFile,
    const fs::path& inputRoot,
    const fs::path& outputRoot,
    Deidentifier& deidentifier
);

void addToSummary(ProcessingSummary& summary, const FileResult& result);

}
#include "file_processor.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "deidentifier.hpp"
#include "metadata.hpp"

namespace deid {

namespace {

std::string normalizedExtension(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return extension;
}

bool isSupportedTextProcessor(const fs::path& path) {
    const std::string extension = normalizedExtension(path);
    return extension == ".txt" || extension == ".md" || extension == ".csv";
}

} // namespace

fs::path getInputFolder(int argc, char* argv[]) {
    if (argc >= 2) {
        return argv[1];
    }

    std::string input;
    std::cout << "Enter the folder path: ";
    std::getline(std::cin, input);
    return input;
}

fs::path createOutputFolder(const fs::path& inputFolder) {
    const fs::path absoluteInput = fs::absolute(inputFolder).lexically_normal();
    const fs::path parent = absoluteInput.parent_path();
    const std::string baseName = absoluteInput.filename().string() + "_deidentified";

    fs::path outputFolder = parent / baseName;
    for (unsigned int suffix = 2; fs::exists(outputFolder); ++suffix) {
        outputFolder = parent / (baseName + "_" + std::to_string(suffix));
    }

    fs::create_directories(outputFolder);
    return outputFolder;
}

std::vector<fs::path> findSupportedFiles(const fs::path& inputFolder) {
    const std::set<std::string> supportedExtensions = {
        ".txt", ".md", ".csv", ".doc", ".docx", ".pdf"
    };

    std::vector<fs::path> files;
    std::error_code error;
    fs::recursive_directory_iterator iterator(
        inputFolder, fs::directory_options::skip_permission_denied, error
    );

    for (const auto end = fs::recursive_directory_iterator(); iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }

        if (!iterator->is_regular_file(error)) {
            error.clear();
            continue;
        }

        if (supportedExtensions.find(normalizedExtension(iterator->path())) != supportedExtensions.end()) {
            files.push_back(iterator->path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

FileResult processFile(
    const fs::path& inputFile,
    const fs::path& inputRoot,
    const fs::path& outputRoot,
    Deidentifier& deidentifier
) {
    if (!isSupportedTextProcessor(inputFile)) {
        return {FileStatus::Skipped, 0,
            "Skipped: format requires binary/document parser (DOC/DOCX/PDF)."};
    }

    std::ifstream input(inputFile, std::ios::binary);
    if (!input) {
        return {FileStatus::Failed, 0, "Failed: could not read file."};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    // 1. Metadata inspection and removal
    const std::string cleanedContent = MetadataProcessor::stripMetadata(buffer.str(), inputFile);

    // 2. Detection, overlap resolution, and consistency-preserving replacement
    const auto deidentified = deidentifier.deidentifyText(cleanedContent);

    std::error_code error;
    const fs::path relativePath = fs::relative(inputFile, inputRoot, error);
    if (error) {
        return {FileStatus::Failed, 0, "Failed: could not calculate output path."};
    }

    const fs::path outputFile = outputRoot / relativePath;
    fs::create_directories(outputFile.parent_path(), error);
    if (error) {
        return {FileStatus::Failed, 0, "Failed: could not create output folder."};
    }

    std::ofstream output(outputFile, std::ios::binary);
    if (!output) {
        return {FileStatus::Failed, 0, "Failed: could not write output file."};
    }

    output << deidentified.text;
    if (!output) {
        return {FileStatus::Failed, 0, "Failed: writing output file was incomplete."};
    }

    return {FileStatus::Processed, deidentified.redactions,
        "Processed: " + std::to_string(deidentified.redactions) + " redaction(s)."};
}

void addToSummary(ProcessingSummary& summary, const FileResult& result) {
    summary.redactions += result.redactions;

    switch (result.status) {
    case FileStatus::Processed:
        ++summary.processed;
        break;
    case FileStatus::Skipped:
        ++summary.skipped;
        break;
    case FileStatus::Failed:
        ++summary.failed;
        break;
    }
}

} // namespace deid

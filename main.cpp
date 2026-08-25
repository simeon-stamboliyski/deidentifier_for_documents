#include <filesystem>
#include <iostream>
#include <string>

#include "deidentifier.hpp"
#include "file_processor.hpp"

namespace fs = std::filesystem;

namespace {

void printSummary(const deid::ProcessingSummary& summary, const fs::path& outputFolder) {
    std::cout << "\nFinished.\n"
              << "  Processed: " << summary.processed << "\n"
              << "  Skipped:   " << summary.skipped << "\n"
              << "  Failed:    " << summary.failed << "\n"
              << "  Redactions: " << summary.redactions << "\n"
              << "  Output:    " << outputFolder.string() << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    const fs::path inputFolder = deid::getInputFolder(argc, argv);

    if (!fs::is_directory(inputFolder)) {
        std::cerr << "Error: the path is not a valid folder.\n";
        return 1;
    }

    const auto files = deid::findSupportedFiles(inputFolder);
    if (files.empty()) {
        std::cout << "No supported documents found to deidentify.\n";
        return 0;
    }

    const fs::path outputFolder = deid::createOutputFolder(inputFolder);
    std::cout << "Found " << files.size() << " supported file(s).\n";
    std::cout << "Writing to: " << outputFolder.string() << "\n\n";

    deid::Deidentifier deidentifier;
    deid::ProcessingSummary summary;

    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        std::cout << "[" << (index + 1) << "/" << files.size() << "] "
                  << file.string() << "\n";

        const auto result = deid::processFile(file, inputFolder, outputFolder, deidentifier);
        deid::addToSummary(summary, result);

        std::cout << "  " << result.message << "\n";
    }

    printSummary(summary, outputFolder);
    return summary.failed == 0 ? 0 : 2;
}

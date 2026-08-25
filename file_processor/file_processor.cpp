#include "file_processor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
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

class TextDocumentParser : public IDocumentParser {
public:
    std::string extractText(const fs::path& filePath) override {
        std::ifstream input(filePath, std::ios::binary);
        if (!input) return "";
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    bool writeText(const fs::path& outputPath, const std::string& content) override {
        std::ofstream output(outputPath, std::ios::binary);
        if (!output) return false;
        output << content;
        return output.good();
    }
};

class CsvDocumentParser : public TextDocumentParser {
public:
    std::string extractText(const fs::path& filePath) override {
        return TextDocumentParser::extractText(filePath);
    }

    bool writeText(const fs::path& outputPath, const std::string& content) override {
        return TextDocumentParser::writeText(outputPath, content);
    }
};

class BinaryDocumentParser : public IDocumentParser {
public:
    explicit BinaryDocumentParser(std::string ext) : ext_(std::move(ext)) {}

    std::string extractText(const fs::path& filePath) override {
        std::string command;
        fs::path tempOutput = fs::temp_directory_path() / "deid_extracted.txt";

        if (ext_ == ".pdf") {
            command = "pdftotext \"" + filePath.string() + "\" \"" + tempOutput.string() + "\" 2>/dev/null";
        } else if (ext_ == ".docx" || ext_ == ".doc") {
            command = "pandoc \"" + filePath.string() + "\" -t plain -o \"" + tempOutput.string() + "\" 2>/dev/null";
        }

        if (!command.empty() && std::system(command.c_str()) == 0 && fs::exists(tempOutput)) {
            TextDocumentParser textParser;
            std::string extracted = textParser.extractText(tempOutput);
            fs::remove(tempOutput);
            if (!extracted.empty()) return extracted;
        }

        TextDocumentParser fallback;
        return fallback.extractText(filePath);
    }

    bool writeText(const fs::path& outputPath, const std::string& content) override {
        fs::path tempTxt = fs::temp_directory_path() / "deid_temp_output.txt";
        {
            std::ofstream tempOut(tempTxt, std::ios::binary);
            if (!tempOut) return false;
            tempOut << content;
        }

        std::string command = "pandoc \"" + tempTxt.string() + "\" -o \"" + outputPath.string() + "\" 2>/dev/null";

        bool success = false;
        if (std::system(command.c_str()) == 0 && fs::exists(outputPath)) {
            success = true;
        } else {
            std::ofstream output(outputPath, std::ios::binary);
            if (output) {
                output << content;
                success = output.good();
            }
        }

        fs::remove(tempTxt);
        return success;
    }

private:
    std::string ext_;
};

} // namespace

std::unique_ptr<IDocumentParser> createParserForExtension(const std::string& extension) {
    if (extension == ".txt" || extension == ".md") {
        return std::make_unique<TextDocumentParser>();
    } else if (extension == ".csv") {
        return std::make_unique<CsvDocumentParser>();
    } else if (extension == ".pdf" || extension == ".doc" || extension == ".docx") {
        return std::make_unique<BinaryDocumentParser>(extension);
    }
    return nullptr;
}

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
    const std::string ext = normalizedExtension(inputFile);
    auto parser = createParserForExtension(ext);
    
    if (!parser) {
        return {FileStatus::Skipped, 0, "Skipped: unhandled file extension."};
    }

    std::string rawContent = parser->extractText(inputFile);
    if (rawContent.empty()) {
        return {FileStatus::Failed, 0, "Failed: could not read or extract document text."};
    }

    const std::string cleanedContent = MetadataProcessor::stripMetadata(rawContent, inputFile);

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

    if (!parser->writeText(outputFile, deidentified.text)) {
        return {FileStatus::Failed, 0, "Failed: writing deidentified document failed."};
    }

    return {FileStatus::Processed, deidentified.redactions,
        "Processed (" + ext + "): " + std::to_string(deidentified.redactions) + " redaction(s)."};
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

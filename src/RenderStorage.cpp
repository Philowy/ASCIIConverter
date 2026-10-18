#include "RenderStorage.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace {
    constexpr uint32_t MAGIC_HEADER = 0x49435341; // "ASCI"
    constexpr uint32_t FILE_VERSION = 1;

    std::string getCurrentTimestampString() {
        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#ifdef _WIN32
        localtime_s(&tmNow, &timeNow);
#else
        localtime_r(&timeNow, &tmNow);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string generateFolderId() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#ifdef _WIN32
        localtime_s(&tmNow, &timeNow);
#else
        localtime_r(&timeNow, &tmNow);
#endif
        std::ostringstream oss;
        oss << "render_" << std::put_time(&tmNow, "%Y%m%d_%H%M%S") << "_" << (ms % 1000);
        return oss.str();
    }
}

RenderStorage::RenderStorage(std::filesystem::path storageDir)
    : m_storageDir(std::move(storageDir)) {
    ensureStorageDirExists();
}

void RenderStorage::ensureStorageDirExists() const {
    std::error_code ec;
    if (!std::filesystem::exists(m_storageDir)) {
        std::filesystem::create_directories(m_storageDir, ec);
    }
}

std::string RenderStorage::formatDuration(double totalSeconds) {
    if (totalSeconds < 0.0) totalSeconds = 0.0;
    int secs = static_cast<int>(totalSeconds);
    int minutes = secs / 60;
    int remainingSecs = secs % 60;
    std::ostringstream oss;
    oss << minutes << ":" << (remainingSecs < 10 ? "0" : "") << remainingSecs;
    return oss.str();
}

std::vector<RenderEntry> RenderStorage::listSavedRenders() const {
    std::vector<RenderEntry> results;
    std::error_code ec;
    if (!std::filesystem::exists(m_storageDir, ec)) {
        return results;
    }

    for (const auto& dirEntry : std::filesystem::directory_iterator(m_storageDir, ec)) {
        if (!dirEntry.is_directory()) continue;

        auto metaPath = dirEntry.path() / "meta.txt";
        auto framesPath = dirEntry.path() / "frames.bin";
        auto audioPath = dirEntry.path() / "audio.ogg";

        if (!std::filesystem::exists(metaPath) || !std::filesystem::exists(framesPath)) {
            continue;
        }

        std::ifstream metaFile(metaPath);
        if (!metaFile.is_open()) continue;

        RenderEntry entry;
        entry.id = dirEntry.path().filename().string();
        entry.folderPath = dirEntry.path();
        entry.framesPath = framesPath;
        entry.audioPath = audioPath;

        std::string line;
        while (std::getline(metaFile, line)) {
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);

            if (key == "title") entry.title = val;
            else if (key == "url") entry.url = val;
            else if (key == "width") {
                try { entry.width = std::stoi(val); } catch (...) {}
            } else if (key == "height") {
                try { entry.height = std::stoi(val); } catch (...) {}
            } else if (key == "fps") {
                try { entry.fps = std::stod(val); } catch (...) {}
            } else if (key == "frames") {
                try { entry.frameCount = std::stoull(val); } catch (...) {}
            } else if (key == "duration") {
                try { entry.durationSeconds = std::stod(val); } catch (...) {}
            } else if (key == "created") entry.createdAt = val;
        }

        if (entry.title.empty()) {
            entry.title = entry.id;
        }

        results.push_back(std::move(entry));
    }

    // Sort newest first by created date or folder name
    std::sort(results.begin(), results.end(), [](const RenderEntry& a, const RenderEntry& b) {
        if (!a.createdAt.empty() && !b.createdAt.empty()) {
            return a.createdAt > b.createdAt;
        }
        return a.id > b.id;
    });

    return results;
}

bool RenderStorage::saveRender(const std::string& title,
                               const std::string& url,
                               int width,
                               int height,
                               double fps,
                               const std::string& sourceAudioPath,
                               const std::vector<std::string>& frames,
                               RenderEntry* outEntry) {
    ensureStorageDirExists();
    std::string folderId = generateFolderId();
    auto folderPath = m_storageDir / folderId;

    std::error_code ec;
    if (!std::filesystem::create_directories(folderPath, ec)) {
        std::cerr << "Blad: Nie udalo sie utworzyc katalogu dla zapisu: " << folderPath << "\n";
        return false;
    }

    auto targetAudioPath = folderPath / "audio.ogg";
    if (std::filesystem::exists(sourceAudioPath)) {
        std::filesystem::copy_file(sourceAudioPath, targetAudioPath,
                                  std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Ostrzezenie: Nie udalo sie skopiowac pliku audio do historii.\n";
        }
    }

    auto metaPath = folderPath / "meta.txt";
    std::ofstream metaFile(metaPath);
    if (!metaFile.is_open()) {
        std::cerr << "Blad: Nie udalo sie utworzyc pliku meta.txt\n";
        return false;
    }

    double duration = (fps > 0.0) ? (static_cast<double>(frames.size()) / fps) : 0.0;
    std::string createdAt = getCurrentTimestampString();

    metaFile << "title=" << title << "\n";
    metaFile << "url=" << url << "\n";
    metaFile << "width=" << width << "\n";
    metaFile << "height=" << height << "\n";
    metaFile << "fps=" << fps << "\n";
    metaFile << "frames=" << frames.size() << "\n";
    metaFile << "duration=" << duration << "\n";
    metaFile << "created=" << createdAt << "\n";
    metaFile.close();

    auto framesPath = folderPath / "frames.bin";
    std::ofstream binFile(framesPath, std::ios::binary);
    if (!binFile.is_open()) {
        std::cerr << "Blad: Nie udalo sie utworzyc pliku frames.bin\n";
        return false;
    }

    uint32_t magic = MAGIC_HEADER;
    uint32_t version = FILE_VERSION;
    uint64_t frameCount = frames.size();

    binFile.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    binFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
    binFile.write(reinterpret_cast<const char*>(&frameCount), sizeof(frameCount));

    for (const auto& frameStr : frames) {
        uint32_t len = static_cast<uint32_t>(frameStr.size());
        binFile.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            binFile.write(frameStr.data(), len);
        }
    }
    binFile.close();

    if (outEntry) {
        outEntry->id = folderId;
        outEntry->title = title;
        outEntry->url = url;
        outEntry->width = width;
        outEntry->height = height;
        outEntry->fps = fps;
        outEntry->frameCount = frames.size();
        outEntry->durationSeconds = duration;
        outEntry->createdAt = createdAt;
        outEntry->folderPath = folderPath;
        outEntry->audioPath = targetAudioPath;
        outEntry->framesPath = framesPath;
    }

    return true;
}

bool RenderStorage::loadRenderFrames(const RenderEntry& entry, std::vector<std::string>& outFrames) const {
    if (!std::filesystem::exists(entry.framesPath)) {
        std::cerr << "Blad: Plik frames.bin nie istnieje: " << entry.framesPath << "\n";
        return false;
    }

    std::ifstream binFile(entry.framesPath, std::ios::binary);
    if (!binFile.is_open()) {
        std::cerr << "Blad: Nie mozna otworzyc frames.bin\n";
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t frameCount = 0;

    binFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    binFile.read(reinterpret_cast<char*>(&version), sizeof(version));
    binFile.read(reinterpret_cast<char*>(&frameCount), sizeof(frameCount));

    if (magic != MAGIC_HEADER || version != FILE_VERSION) {
        std::cerr << "Blad: Niepoprawny format lub wersja pliku frames.bin\n";
        return false;
    }

    outFrames.clear();
    outFrames.resize(frameCount);

    for (uint64_t i = 0; i < frameCount; ++i) {
        uint32_t len = 0;
        binFile.read(reinterpret_cast<char*>(&len), sizeof(len));
        outFrames[i].resize(len);
        if (len > 0) {
            binFile.read(outFrames[i].data(), len);
        }
    }

    return true;
}

bool RenderStorage::deleteRender(const RenderEntry& entry) const {
    std::error_code ec;
    return std::filesystem::remove_all(entry.folderPath, ec) > 0;
}

#include "YouTubeFetcher.hpp"
#include <array>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <vector>

std::string YouTubeFetcher::executeCommand(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string YouTubeFetcher::getVideoStreamUrl(const std::string& youtubeUrl) {
    std::string cmd = "yt-dlp --no-playlist -f bestvideo -g \"" + youtubeUrl + "\"";
    return executeCommand(cmd);
}

std::string YouTubeFetcher::downloadVideo(const std::string& youtubeUrl) {
    std::cout << "[1/2] Pobieranie wideo z YouTube...\n";

    const std::vector<std::string> extensions = {".mp4", ".mkv", ".webm"};
    for (const auto& ext : extensions) {
        std::error_code ec;
        std::filesystem::remove("temp_video" + ext, ec);
    }

    // Try downloading with 720p limit (fast download, high enough resolution for terminal ASCII)
    std::string cmd = "yt-dlp --no-playlist --force-overwrites -f \"bestvideo[height<=720][ext=mp4]/bestvideo[ext=mp4]/best[ext=mp4]/bestvideo/best\" --remux-video mp4 -o \"temp_video.mp4\" \"" + youtubeUrl + "\"";
    int exitCode = std::system(cmd.c_str());

    if (exitCode != 0 || !std::filesystem::exists("temp_video.mp4")) {
        // Fallback without strict format or remux
        cmd = "yt-dlp --no-playlist --force-overwrites -f \"bestvideo[height<=720]/bestvideo/best\" -o \"temp_video.%(ext)s\" \"" + youtubeUrl + "\"";
        exitCode = std::system(cmd.c_str());
    }

    for (const auto& ext : extensions) {
        std::string filename = "temp_video" + ext;
        if (std::filesystem::exists(filename)) {
            std::cout << "Pobrano wideo do pliku: " << filename << "\n\n";
            return filename;
        }
    }

    std::cerr << "Blad: Nie udalo sie pobrac pliku wideo.\n";
    return "";
}

std::string YouTubeFetcher::downloadAudio(const std::string& youtubeUrl) {
    std::cout << "[2/2] Pobieranie sciezki dzwiekowej z YouTube...\n";

    std::error_code ec;
    std::filesystem::remove("temp_audio.ogg", ec);

    std::string cmd = "yt-dlp --no-playlist --force-overwrites -f bestaudio -x --audio-format vorbis -o \"temp_audio.%(ext)s\" \"" + youtubeUrl + "\"";
    int exitCode = std::system(cmd.c_str());

    if (exitCode != 0 || !std::filesystem::exists("temp_audio.ogg")) {
        std::cerr << "Blad: Nie udalo sie pobrac lub skonwertowac dzwieku.\n";
        return "";
    }

    std::cout << "Pobrano dzwiek do pliku: temp_audio.ogg\n\n";
    return "temp_audio.ogg";
}

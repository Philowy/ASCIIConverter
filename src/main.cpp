#include "AsciiRenderer.hpp"
#include "YouTubeFetcher.hpp"
#include "MediaSynchronizer.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
void enableVirtualTerminalProcessing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}
#else
void enableVirtualTerminalProcessing() {}
#endif

int main(int argc, char* argv[]) {
    enableVirtualTerminalProcessing();

    std::cout << "========================================\n";
    std::cout << "      YouTube ASCII Video Player        \n";
    std::cout << "========================================\n\n";

    std::string youtubeUrl;
    if (argc >= 2) {
        youtubeUrl = argv[1];
        std::cout << "Uzywam podanego adresu URL: " << youtubeUrl << "\n";
    }

    while (youtubeUrl.empty()) {
        std::cout << "Wklej link do filmu na YouTube: ";
        std::getline(std::cin, youtubeUrl);

        // Trim leading and trailing whitespace
        youtubeUrl.erase(0, youtubeUrl.find_first_not_of(" \t\r\n"));
        if (!youtubeUrl.empty()) {
            auto end = youtubeUrl.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                youtubeUrl = youtubeUrl.substr(0, end + 1);
            }
        }
    }

    int width = 120;
    int height = 40;

    std::string input;
    std::cout << "Podaj szerokosc renderu w znakach (domyslnie " << width << "): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            int w = std::stoi(input);
            if (w > 0) width = w;
        } catch (...) {}
    }

    std::cout << "Podaj wysokosc renderu w wierszach (domyslnie " << height << "): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            int h = std::stoi(input);
            if (h > 0) height = h;
        } catch (...) {}
    }

    std::cout << "\nUstawiona rozdzielczosc ASCII: " << width << " x " << height << "\n\n";

    YouTubeFetcher fetcher;
    std::string videoPath = fetcher.downloadVideo(youtubeUrl);
    std::string audioPath = fetcher.downloadAudio(youtubeUrl);

    if (videoPath.empty() || audioPath.empty()) {
        std::cerr << "Blad: Nie udalo sie pobrac wideo lub audio.\n";
        std::cout << "Nacisnij Enter, aby zakonczyc...";
        std::cin.get();
        return 1;
    }

    MediaSynchronizer audio;
    bool audioLoaded = audio.loadAudio(audioPath);
    if (!audioLoaded) {
        std::cerr << "Ostrzezenie: Nie udalo sie zaladowac sciezki dzwiekowej. Odtwarzanie bedzie bez dzwieku.\n";
    }

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Blad: Nie udalo sie otworzyc pliku wideo (" << videoPath << ").\n";
        return 1;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0.0) fps = 30.0;
    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    std::cout << "Rozpoczynam renderowanie klatek do ASCII przed odtworzeniem...\n";
    if (totalFrames > 0) {
        std::cout << "Wykryto klatek: " << totalFrames << " (FPS: " << fps << ")\n";
    }

    AsciiRenderer renderer(width, height);
    std::vector<std::string> asciiFrames;
    if (totalFrames > 0) {
        asciiFrames.reserve(totalFrames);
    }

    cv::Mat frame;
    int renderedCount = 0;
    while (cap.read(frame)) {
        if (frame.empty()) break;
        asciiFrames.push_back(renderer.convertFrameToAscii(frame));
        renderedCount++;

        if (totalFrames > 0 && (renderedCount % 30 == 0 || renderedCount == totalFrames)) {
            int pct = (renderedCount * 100) / totalFrames;
            std::cout << "\rPostep renderowania: " << pct << "% (" << renderedCount << "/" << totalFrames << " klatek)" << std::flush;
        } else if (renderedCount % 30 == 0) {
            std::cout << "\rWyrenderowano: " << renderedCount << " klatek..." << std::flush;
        }
    }
    std::cout << "\n\nRenderowanie zakonczone sukcesem! Lacznie klatek: " << asciiFrames.size() << "\n";
    cap.release();

    if (asciiFrames.empty()) {
        std::cerr << "Blad: Brak klatek do wyswietlenia.\n";
        return 1;
    }

    std::cout << "Odtwarzanie filmu rozpocznie sie za 2 sekundy...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Clear screen and hide cursor
    std::cout << "\x1b[2J\x1b[?25l" << std::flush;

    if (audioLoaded) {
        audio.playAudio();
    }
    auto playbackStartTime = std::chrono::steady_clock::now();

    size_t lastDisplayedIndex = static_cast<size_t>(-1);
    const size_t totalAsciiFrames = asciiFrames.size();

    while (true) {
        double elapsedSeconds = 0.0;
        if (audioLoaded && audio.isPlaying()) {
            elapsedSeconds = audio.getPlayingOffset();
        } else {
            auto now = std::chrono::steady_clock::now();
            elapsedSeconds = std::chrono::duration<double>(now - playbackStartTime).count();
        }

        size_t targetIndex = static_cast<size_t>(elapsedSeconds * fps);
        if (targetIndex >= totalAsciiFrames) {
            break;
        }

        if (targetIndex != lastDisplayedIndex) {
            lastDisplayedIndex = targetIndex;
            renderer.renderAscii(asciiFrames[targetIndex]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (audioLoaded) {
        audio.stopAudio();
    }

    // Restore cursor and color
    std::cout << "\x1b[?25h\x1b[0m\n" << std::flush;
    std::cout << "Odtwarzanie zakonczone.\n";

    return 0;
}

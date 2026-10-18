#include "AsciiRenderer.hpp"
#include "YouTubeFetcher.hpp"
#include "MediaSynchronizer.hpp"
#include "RenderStorage.hpp"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
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
#include <unistd.h>
#include <termios.h>
void enableVirtualTerminalProcessing() {}
int _kbhit() { return 0; }
int _getch() { return 0; }
#endif

void playAsciiVideo(const std::vector<std::string>& asciiFrames,
                    const std::string& audioPath,
                    double fps,
                    const AsciiRenderer& renderer) {
    if (asciiFrames.empty()) {
        std::cerr << "Blad: Brak klatek do odtworzenia.\n";
        return;
    }

    MediaSynchronizer audio;
    bool audioLoaded = false;
    if (!audioPath.empty() && std::filesystem::exists(audioPath)) {
        audioLoaded = audio.loadAudio(audioPath);
    }

    std::cout << "\nOdtwarzanie rozpocznie sie za 1 sekunde...\n";
    std::cout << "(Klawisz ESC lub 'q' = wyjscie, SPACJA = pauza)\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Clear screen and hide cursor
    std::cout << "\x1b[2J\x1b[?25l" << std::flush;

    if (audioLoaded) {
        audio.playAudio();
    }
    auto playbackStartTime = std::chrono::steady_clock::now();

    size_t lastDisplayedIndex = static_cast<size_t>(-1);
    const size_t totalAsciiFrames = asciiFrames.size();
    bool paused = false;
    auto pauseStartTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> totalPausedDuration(0.0);

    while (true) {
        // Keyboard controls
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 27 || ch == 'q' || ch == 'Q') {
                break; // Quit playback
            }
            if (ch == ' ') {
                paused = !paused;
                if (paused) {
                    if (audioLoaded) audio.stopAudio();
                    pauseStartTime = std::chrono::steady_clock::now();
                } else {
                    totalPausedDuration += std::chrono::steady_clock::now() - pauseStartTime;
                    if (audioLoaded) audio.playAudio();
                }
            }
        }

        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        double elapsedSeconds = 0.0;
        if (audioLoaded && audio.isPlaying()) {
            elapsedSeconds = audio.getPlayingOffset();
        } else {
            auto now = std::chrono::steady_clock::now();
            elapsedSeconds = std::chrono::duration<double>(now - playbackStartTime - totalPausedDuration).count();
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
    std::cout << "Odtwarzanie zakonczone.\n\n";
}

void handleNewRender(const std::string& initialUrl = "") {
    std::string youtubeUrl = initialUrl;

    while (youtubeUrl.empty()) {
        std::cout << "Wklej link do filmu na YouTube: ";
        std::getline(std::cin, youtubeUrl);

        youtubeUrl.erase(0, youtubeUrl.find_first_not_of(" \t\r\n"));
        if (!youtubeUrl.empty()) {
            auto end = youtubeUrl.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                youtubeUrl = youtubeUrl.substr(0, end + 1);
            }
        }
    }

    YouTubeFetcher fetcher;
    std::cout << "\nPobieram informacje o filmie...\n";
    std::string videoTitle = fetcher.getVideoTitle(youtubeUrl);
    std::cout << "Tytul filmu: \"" << videoTitle << "\"\n\n";

    int width = 480;
    int height = 160;

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

    std::string videoPath = fetcher.downloadVideo(youtubeUrl);
    std::string audioPath = fetcher.downloadAudio(youtubeUrl);

    if (videoPath.empty()) {
        std::cerr << "Blad: Nie udalo sie pobrac wideo.\n";
        return;
    }

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Blad: Nie udalo sie otworzyc pliku wideo (" << videoPath << ").\n";
        return;
    }

    double sourceFps = cap.get(cv::CAP_PROP_FPS);
    if (sourceFps <= 0.0) sourceFps = 30.0;
    int totalSourceFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    std::cout << "Zrodlowy FPS filmu: " << sourceFps << "\n";
    std::cout << "Podaj docelowy FPS renderu (wcisnij Enter dla " << sourceFps << ", np. 15, 24, 30): ";
    std::string fpsInput;
    std::getline(std::cin, fpsInput);
    double targetFps = sourceFps;
    if (!fpsInput.empty()) {
        try {
            double customFps = std::stod(fpsInput);
            if (customFps > 0.0) targetFps = customFps;
        } catch (...) {}
    }

    std::cout << "Wybrany FPS renderu: " << targetFps << "\n\n";

    unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency());
    std::cout << "Rozpoczynam wielowatkowy pre-render ASCII...\n";
    std::cout << "Liczba aktywnych watkow procesora: " << threadCount << "\n";

    AsciiRenderer renderer(width, height);
    std::vector<std::string> asciiFrames;

    int estimatedTargetFrames = (totalSourceFrames > 0)
        ? static_cast<int>(totalSourceFrames * (targetFps / sourceFps))
        : 0;
    if (estimatedTargetFrames > 0) {
        asciiFrames.reserve(estimatedTargetFrames + 100);
    }

    auto renderStartTime = std::chrono::steady_clock::now();

    double targetFrameInterval = 1.0 / targetFps;
    double nextScheduledTime = 0.0;
    int sourceFrameIdx = 0;

    constexpr size_t BATCH_SIZE = 120;
    std::vector<cv::Mat> currentBatch;
    currentBatch.reserve(BATCH_SIZE);

    cv::Mat frame;
    while (cap.read(frame)) {
        if (frame.empty()) break;

        double currentFrameTime = sourceFrameIdx / sourceFps;
        if (currentFrameTime + 1e-4 >= nextScheduledTime) {
            currentBatch.push_back(std::move(frame));
            nextScheduledTime += targetFrameInterval;

            if (currentBatch.size() >= BATCH_SIZE) {
                std::vector<std::string> batchResults;
                renderer.convertBatchParallel(currentBatch, batchResults);
                for (auto& s : batchResults) {
                    asciiFrames.push_back(std::move(s));
                }
                currentBatch.clear();

                if (estimatedTargetFrames > 0) {
                    int pct = static_cast<int>((asciiFrames.size() * 100) / estimatedTargetFrames);
                    if (pct > 100) pct = 100;
                    std::cout << "\rPostep renderowania: " << pct << "% (" << asciiFrames.size() << " klatek)" << std::flush;
                } else {
                    std::cout << "\rWyrenderowano: " << asciiFrames.size() << " klatek..." << std::flush;
                }
            }
        }
        sourceFrameIdx++;
    }

    // Process leftover frames in last batch
    if (!currentBatch.empty()) {
        std::vector<std::string> batchResults;
        renderer.convertBatchParallel(currentBatch, batchResults);
        for (auto& s : batchResults) {
            asciiFrames.push_back(std::move(s));
        }
        currentBatch.clear();
    }

    cap.release();

    auto renderEndTime = std::chrono::steady_clock::now();
    double renderTimeSeconds = std::chrono::duration<double>(renderEndTime - renderStartTime).count();

    std::cout << "\n\nRenderowanie zakonczone! Wyrenderowano " << asciiFrames.size()
              << " klatek w " << std::fixed << std::setprecision(2) << renderTimeSeconds << " s";
    if (renderTimeSeconds > 0) {
        std::cout << " (" << static_cast<int>(asciiFrames.size() / renderTimeSeconds) << " FPS renderowania)\n";
    } else {
        std::cout << "\n";
    }

    // Save render to history
    RenderStorage storage;
    RenderEntry savedEntry;
    std::cout << "Zapisywanie renderu w historii...\n";
    if (storage.saveRender(videoTitle, youtubeUrl, width, height, targetFps, audioPath, asciiFrames, &savedEntry)) {
        std::cout << "Zapisano pomyslnie do historii! (Katalog: " << savedEntry.id << ")\n";
    } else {
        std::cerr << "Ostrzezenie: Nie udalo sie zapisac renderu do historii.\n";
    }

    // Playback loop
    while (true) {
        playAsciiVideo(asciiFrames, audioPath, targetFps, renderer);

        std::cout << "Opcje powtorki:\n";
        std::cout << "[1] Odtworz ponownie\n";
        std::cout << "[0] Wroc do glownego menu\n";
        std::cout << "Wybor: ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice != "1") {
            break;
        }
    }
}

void handleHistoryMenu() {
    RenderStorage storage;

    while (true) {
        std::system("cls");
        auto renders = storage.listSavedRenders();

        std::cout << "\n========================================\n";
        std::cout << "       HISTORIA ZAPISANYCH RENDEROW     \n";
        std::cout << "========================================\n";

        if (renders.empty()) {
            std::cout << "Brak zapisanych renderow w historii.\n";
            std::cout << "Nacisnij Enter, aby powrocic do menu glownego...";
            std::string dummy;
            std::getline(std::cin, dummy);
            std::system("cls");
            return;
        }

        for (size_t i = 0; i < renders.size(); ++i) {
            const auto& r = renders[i];
            int mins = static_cast<int>(r.durationSeconds) / 60;
            int secs = static_cast<int>(r.durationSeconds) % 60;

            std::cout << "[" << (i + 1) << "] " << r.title << "\n";
            std::cout << "    Wymiary: " << r.width << "x" << r.height
                      << " | FPS: " << r.fps
                      << " | Czas: " << mins << ":" << (secs < 10 ? "0" : "") << secs
                      << " (" << r.frameCount << " klatek)\n";
            std::cout << "    Zapisano: " << r.createdAt << "\n";
            std::cout << "----------------------------------------\n";
        }

        std::cout << "\nWybierz numer filmu [1-" << renders.size() << "] do odtworzenia,\n";
        std::cout << "'u <numer>' aby usunac wpis, lub '0' aby wrocic: ";

        std::string choice;
        std::getline(std::cin, choice);

        choice.erase(0, choice.find_first_not_of(" \t\r\n"));
        if (choice.empty() || choice == "0") {
            return;
        }

        // Check if user wants to delete
        if ((choice[0] == 'u' || choice[0] == 'U' || choice[0] == 'd' || choice[0] == 'D') && choice.size() > 1) {
            std::string numPart = choice.substr(1);
            numPart.erase(0, numPart.find_first_not_of(" \t"));
            try {
                int idx = std::stoi(numPart) - 1;
                if (idx >= 0 && idx < static_cast<int>(renders.size())) {
                    std::cout << "Czy na pewno chcesz usunac: \"" << renders[idx].title << "\"? (t/n): ";
                    std::string confirm;
                    std::getline(std::cin, confirm);
                    if (confirm == "t" || confirm == "T" || confirm == "y" || confirm == "Y") {
                        storage.deleteRender(renders[idx]);
                        std::cout << "Usunieto pomyslnie.\n";
                    }
                }
            } catch (...) {}
            continue;
        }

        // Try selecting index to play
        try {
            int selectedIdx = std::stoi(choice) - 1;
            if (selectedIdx >= 0 && selectedIdx < static_cast<int>(renders.size())) {
                const auto& entry = renders[selectedIdx];
                std::cout << "\nWczytuje wyrenderowane klatki z dysku...\n";
                auto loadStart = std::chrono::steady_clock::now();

                std::vector<std::string> loadedFrames;
                if (!storage.loadRenderFrames(entry, loadedFrames)) {
                    std::cerr << "Blad podczas odczytu pliku klatek.\n";
                    continue;
                }

                auto loadEnd = std::chrono::steady_clock::now();
                double loadSeconds = std::chrono::duration<double>(loadEnd - loadStart).count();
                std::cout << "Wczytano " << loadedFrames.size() << " klatek w "
                          << std::fixed << std::setprecision(2) << loadSeconds << " s!\n";

                AsciiRenderer renderer(entry.width, entry.height);

                while (true) {
                    playAsciiVideo(loadedFrames, entry.audioPath.string(), entry.fps, renderer);

                    std::cout << "Opcje powtorki:\n";
                    std::cout << "[1] Odtworz ponownie\n";
                    std::cout << "[0] Wroc do listy historii\n";
                    std::cout << "Wybor: ";
                    std::string replayChoice;
                    std::getline(std::cin, replayChoice);
                    if (replayChoice != "1") {
                        break;
                    }
                }
            } else {
                std::cout << "Nieprawidlowy numer.\n";
            }
        } catch (...) {
            std::cout << "Nieprawidlowy wybor.\n";
        }
        std::system("cls");
    }
}

int main(int argc, char* argv[]) {
    enableVirtualTerminalProcessing();

    // If URL was passed via command line argument, jump directly to new render
    if (argc >= 2) {
        std::cout << "========================================\n";
        std::cout << "      YouTube ASCII Video Player        \n";
        std::cout << "========================================\n\n";
        handleNewRender(argv[1]);
        return 0;
    }

    while (true) {
        std::cout << "\n========================================\n";
        std::cout << "      YouTube ASCII Video Player        \n";
        std::cout << "========================================\n\n";
        std::cout << "[1] Pobierz i wyrenderuj nowy film z YouTube\n";
        std::cout << "[2] Historia zapisanych renderow (odtworz od razu)\n";
        std::cout << "[3] Wyjdz\n";
        std::cout << "Wybor: ";

        std::string menuChoice;
        std::getline(std::cin, menuChoice);

        menuChoice.erase(0, menuChoice.find_first_not_of(" \t\r\n"));

        if (menuChoice == "1") {
            handleNewRender();
        } else if (menuChoice == "2") {
            handleHistoryMenu();
        } else if (menuChoice == "3" || menuChoice == "q" || menuChoice == "Q") {
            std::cout << "Do zobaczenia!\n";
            break;
        } else {
            std::cout << "Nieznana opcja, sprobuj ponownie.\n";
        }
    }

    return 0;
}

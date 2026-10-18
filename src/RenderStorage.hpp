#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

struct RenderEntry {
    std::string id;
    std::string title;
    std::string url;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    size_t frameCount = 0;
    double durationSeconds = 0.0;
    std::string createdAt;
    std::filesystem::path folderPath;
    std::filesystem::path audioPath;
    std::filesystem::path framesPath;
};

class RenderStorage {
public:
    explicit RenderStorage(std::filesystem::path storageDir = "renders");

    [[nodiscard]] std::vector<RenderEntry> listSavedRenders() const;

    bool saveRender(const std::string& title,
                    const std::string& url,
                    int width,
                    int height,
                    double fps,
                    const std::string& sourceAudioPath,
                    const std::vector<std::string>& frames,
                    RenderEntry* outEntry = nullptr);

    bool loadRenderFrames(const RenderEntry& entry, std::vector<std::string>& outFrames) const;

    bool deleteRender(const RenderEntry& entry) const;

private:
    std::filesystem::path m_storageDir;
    void ensureStorageDirExists() const;
    static std::string formatDuration(double totalSeconds);
};

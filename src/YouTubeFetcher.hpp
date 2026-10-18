#pragma once
#include <string>

class YouTubeFetcher {
public:
    std::string getVideoTitle(const std::string& youtubeUrl);
    std::string downloadVideo(const std::string& youtubeUrl);
    std::string downloadAudio(const std::string& youtubeUrl);
    std::string getVideoStreamUrl(const std::string& youtubeUrl);
private:
    std::string executeCommand(const std::string& cmd);
};

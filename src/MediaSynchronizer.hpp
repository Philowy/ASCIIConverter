#pragma once
#include <SFML/Audio.hpp>
#include <string>

class MediaSynchronizer {
public:
    bool loadAudio(const std::string& audioUrl);
    void playAudio();
    void stopAudio();
    [[nodiscard]] float getPlayingOffset() const;
    [[nodiscard]] bool isPlaying() const;

private:
    sf::Music music;
};

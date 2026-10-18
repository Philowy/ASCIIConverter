#include "MediaSynchronizer.hpp"
#include <iostream>

bool MediaSynchronizer::loadAudio(const std::string& audioUrl) {
    if (!music.openFromFile(audioUrl)) {
        std::cerr << "Nie udalo sie otworzyc pliku dzwiekowego: " << audioUrl << "\n";
        return false;
    }
    return true;
}

void MediaSynchronizer::playAudio() {
    music.play();
}

void MediaSynchronizer::stopAudio() {
    music.stop();
}

float MediaSynchronizer::getPlayingOffset() const {
    return music.getPlayingOffset().asSeconds();
}

bool MediaSynchronizer::isPlaying() const {
    return music.getStatus() == sf::Music::Status::Playing;
}

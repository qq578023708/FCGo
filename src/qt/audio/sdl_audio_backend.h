#pragma once

#include "iaudio_backend.h"
#include <mutex>
#include <SDL2/SDL.h>

// ============================================================
// SDLAudioBackend — SDL2 implementation of IAudioBackend
// ============================================================
class SDLAudioBackend : public IAudioBackend {
public:
    SDLAudioBackend() = default;
    ~SDLAudioBackend() override;

    bool init(int sampleRate = 44100, int channels = 1) override;
    void shutdown() override;
    void pushSamples(const std::vector<float>& samples) override;
    void setVolume(float volume) override { volume_ = volume; }
    float volume() const override { return volume_; }
    bool isActive() const override { return active_; }
    const char* name() const override { return "SDL2 Audio"; }

private:
    static void audioCallback(void* userdata, uint8_t* stream, int len);

    SDL_AudioDeviceID device_ = 0;
    float volume_ = 1.0f;
    int sampleRate_ = 44100;
    std::mutex mutex_;
    std::vector<float> sampleBuffer_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
    size_t underrunCount_ = 0;
    bool active_ = false;
};

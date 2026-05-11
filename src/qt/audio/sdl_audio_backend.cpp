#include "sdl_audio_backend.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// Target latency: ~20ms (good balance between low latency and underrun protection)
static constexpr int kTargetLatencyMs = 20;
static constexpr int kMaxBufferSamples = 44100 * 2;  // Max 2 seconds buffer

SDLAudioBackend::~SDLAudioBackend() {
    shutdown();
}

bool SDLAudioBackend::init(int sampleRate, int channels) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[Audio] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want, have;
    std::memset(&want, 0, sizeof(want));
    want.freq = sampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = channels;
    // Calculate samples for target latency: sampleRate * latencyMs / 1000
    // Must be power of 2 for some audio drivers
    want.samples = 512;  // ~11.6ms @ 44100Hz
    want.callback = audioCallback;
    want.userdata = this;

    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (device_ == 0) {
        fprintf(stderr, "[Audio] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    active_ = true;
    sampleRate_ = have.freq;
    
    // Pre-allocate buffer for ~100ms of audio
    sampleBuffer_.reserve(sampleRate_ / 10);
    
    SDL_PauseAudioDevice(device_, 0);

    fprintf(stdout, "[Audio] Initialized: %dHz, %dch, buffer=%d samples (~%.1fms)\n",
            have.freq, have.channels, have.samples, 
            have.samples * 1000.0f / have.freq);
    return true;
}

void SDLAudioBackend::shutdown() {
    if (device_ != 0) {
        active_ = false;
        SDL_PauseAudioDevice(device_, 1);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sampleBuffer_.clear();
    }
    readPos_ = 0;
    writePos_ = 0;
}

void SDLAudioBackend::pushSamples(const std::vector<float>& samples) {
    if (!active_ || samples.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    
    // Ensure buffer has enough space
    size_t needed = writePos_ + samples.size();
    if (needed > sampleBuffer_.size()) {
        sampleBuffer_.resize(needed);
    }
    
    // Copy samples to buffer
    std::memcpy(&sampleBuffer_[writePos_], samples.data(), samples.size() * sizeof(float));
    writePos_ += samples.size();
    
    // Dynamic sync: if buffer grows too large, drop excess to maintain target latency
    int targetSamples = sampleRate_ * kTargetLatencyMs / 1000;
    if ((int)(writePos_ - readPos_) > targetSamples * 2) {
        // Buffer too full, drop samples to get back to target
        size_t excess = (writePos_ - readPos_) - targetSamples;
        readPos_ += excess;
        underrunCount_ = 0;  // Reset underrun counter
    }
    
    // Hard limit: prevent buffer from growing indefinitely
    if (writePos_ > kMaxBufferSamples) {
        // Compact buffer by moving valid data to front
        size_t valid = writePos_ - readPos_;
        if (valid > 0 && readPos_ > 0) {
            std::memmove(&sampleBuffer_[0], &sampleBuffer_[readPos_], valid * sizeof(float));
        }
        writePos_ = valid;
        readPos_ = 0;
    }
}

void SDLAudioBackend::audioCallback(void* userdata, uint8_t* stream, int len) {
    auto* self = static_cast<SDLAudioBackend*>(userdata);
    float* out = reinterpret_cast<float*>(stream);
    int numFrames = len / sizeof(float);

    std::lock_guard<std::mutex> lock(self->mutex_);
    size_t available = self->writePos_ - self->readPos_;
    
    if (available >= (size_t)numFrames) {
        // Enough data: copy to output
        for (int i = 0; i < numFrames; i++) {
            out[i] = self->sampleBuffer_[self->readPos_ + i] * self->volume_;
        }
        self->readPos_ += numFrames;
        self->underrunCount_ = 0;
    } else {
        // Underrun: copy what we have, fill rest with silence
        for (size_t i = 0; i < available; i++) {
            out[i] = self->sampleBuffer_[self->readPos_ + i] * self->volume_;
        }
        for (int i = available; i < numFrames; i++) {
            out[i] = 0.0f;
        }
        self->readPos_ = self->writePos_;
        self->underrunCount_++;
        
        // Log underrun occasionally
        if (self->underrunCount_ == 1 || self->underrunCount_ % 60 == 0) {
            fprintf(stderr, "[Audio] Underrun #%zu (buffer: %zu samples)\n", 
                    self->underrunCount_, available);
        }
    }
}

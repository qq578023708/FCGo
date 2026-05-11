#pragma once

#include <cstdint>
#include <vector>
#include <string>

// ============================================================
// IAudioBackend — Abstract audio backend interface
// Allows runtime switching between different audio implementations
// ============================================================
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // Initialize audio device
    virtual bool init(int sampleRate = 44100, int channels = 1) = 0;
    
    // Shutdown audio device
    virtual void shutdown() = 0;
    
    // Push audio samples (float, -1.0 to 1.0)
    virtual void pushSamples(const std::vector<float>& samples) = 0;
    
    // Volume control (0.0 to 1.0)
    virtual void setVolume(float volume) = 0;
    virtual float volume() const = 0;
    
    // Check if audio is active
    virtual bool isActive() const = 0;
    
    // Get backend name
    virtual const char* name() const = 0;
};

// ============================================================
// AudioBackendType — Enum for available backends
// ============================================================
enum class AudioBackendType {
    SDL2,       // SDL2 Audio (cross-platform)
    Null        // Silent (no output)
};

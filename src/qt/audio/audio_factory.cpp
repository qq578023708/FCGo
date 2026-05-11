#include "audio_factory.h"
#include "sdl_audio_backend.h"
#include <algorithm>
#include <cctype>

std::unique_ptr<IAudioBackend> AudioFactory::create(AudioBackendType type) {
    switch (type) {
    case AudioBackendType::SDL2:
        return std::make_unique<SDLAudioBackend>();
    case AudioBackendType::Null:
        return nullptr;  // Null audio - no backend
    default:
        return std::make_unique<SDLAudioBackend>();
    }
}

AudioBackendType AudioFactory::fromString(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "sdl2" || lower == "sdl") return AudioBackendType::SDL2;
    if (lower == "null" || lower == "none" || lower == "silent") return AudioBackendType::Null;
    
    return AudioBackendType::SDL2;
}

const char* AudioFactory::toString(AudioBackendType type) {
    switch (type) {
    case AudioBackendType::SDL2: return "SDL2";
    case AudioBackendType::Null: return "Null";
    default: return "Unknown";
    }
}

bool AudioFactory::isAvailable(AudioBackendType type) {
    switch (type) {
    case AudioBackendType::SDL2:
        return true;  // SDL2 is always available
    case AudioBackendType::Null:
        return true;  // Null is always available
    default:
        return false;
    }
}

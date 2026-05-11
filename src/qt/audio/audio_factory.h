#pragma once

#include "iaudio_backend.h"
#include <memory>
#include <string>

// ============================================================
// AudioFactory — Creates audio backend instances
// Supports runtime switching via settings
// ============================================================
class AudioFactory {
public:
    // Create audio backend by type
    static std::unique_ptr<IAudioBackend> create(AudioBackendType type);
    
    // Convert string to type (for settings)
    static AudioBackendType fromString(const std::string& s);
    
    // Convert type to string (for settings)
    static const char* toString(AudioBackendType type);
    
    // Get default type
    static AudioBackendType defaultType() { return AudioBackendType::SDL2; }
    
    // Check if type is available on this platform
    static bool isAvailable(AudioBackendType type);
};

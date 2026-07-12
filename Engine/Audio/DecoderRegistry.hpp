/**
 * @file           : DecoderRegistry.hpp
 * @author         : Romi Brooks
 * @brief          : Factory-based registry for audio decoder backends
 * @attention      : Backends register themselves at startup via static init
 * @date           : 2026/7/12
  Copyright (c) 2026 Romi Brooks,  All rights reserved.
**/

#ifndef ATOM_DECODER_REGISTRY_HPP
#define ATOM_DECODER_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <Engine/Interfaces/IAudioDecoder.hpp>

namespace atom {

class DecoderRegistry {
public:
    using Factory = std::function<std::unique_ptr<IAudioDecoder>()>;

    static auto Register(const std::string& extension, Factory factory) -> void;
    static auto Create(const std::string& filepath) -> std::unique_ptr<IAudioDecoder>;

private:
    static auto GetFactories() -> std::unordered_map<std::string, Factory>&;
};

} // namespace atom

#endif // ATOM_DECODER_REGISTRY_HPP

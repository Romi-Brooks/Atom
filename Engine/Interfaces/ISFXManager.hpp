#ifndef ATOM_ISFX_MANAGER_HPP
#define ATOM_ISFX_MANAGER_HPP

#include <cstddef>
#include <string>

#include <Engine/Interfaces/IAudioBuffer.hpp>

namespace atom {

class ISFXManager {
public:
    virtual ~ISFXManager() = default;

    virtual auto Load(const std::string& id, const std::string& filePath) -> bool = 0;
    [[nodiscard]] virtual auto GetBuffer(const std::string& id) -> IAudioBuffer* = 0;
    [[nodiscard]] virtual auto Has(const std::string& id) const -> bool = 0;
    virtual auto Unload(const std::string& id) -> bool = 0;
    virtual auto UnloadAll() -> void = 0;
    [[nodiscard]] virtual auto GetLoadedCount() const -> size_t = 0;
};

} // namespace atom

#endif // ATOM_ISFX_MANAGER_HPP

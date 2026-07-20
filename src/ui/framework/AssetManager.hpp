// This component uses type erasure by holding pointers to the empty base class IAsset
// The AssetManager can load any graphical resource (such as images, fonts, or sound clips) in a decoupled way,
// and provide access to them through dynamic_cast to the concrete type only when drawing in the Backend
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <type_traits>

// Empty base class for representing an abstract resource in memory
class IAsset {
public:
    virtual ~IAsset() = default;
};

class AssetManager {
private:
    // Mapping from a unique textual identifier to its abstract resource in memory
    std::unordered_map<std::string, std::unique_ptr<IAsset>> m_assets;

public:
    AssetManager() = default;
    ~AssetManager() = default;

    // Prevent copying the asset manager to avoid duplicate resources and memory ownership issues
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    
    AssetManager(AssetManager&&) noexcept = default;
    AssetManager& operator=(AssetManager&&) noexcept = default;

    /**
     * Load and register a new resource in the system.
     * @tparam T The concrete resource type (must inherit from IAsset).
     * @param assetId The unique textual identifier used to access the resource.
     * @param args Parameters passed to the concrete resource constructor (for example a file path).
     */
    template <typename T, typename... Args>
    void loadAsset(const std::string& assetId, Args&&... args) {
        static_assert(std::is_base_of_v<IAsset, T>, "T must derive from IAsset");
        
        if (m_assets.find(assetId) != m_assets.end()) {
            return; // The resource already exists in memory, no need to load it again
        }

        // Construct the object before inserting it into the map, instead of
        // m_assets[assetId] = std::make_unique<T>(...).
        // If construction throws an exception (for example a missing image file), it is important that the map does not contain
        // an empty entry for this assetId — otherwise the check above will think the asset "already
        // loaded" on every future call, and the asset will never be loaded again in the same run,
        // even if the actual fix (for example adding the missing file) has already been done.
        auto asset = std::make_unique<T>(std::forward<Args>(args)...);
        m_assets.emplace(assetId, std::move(asset));
    }

    /**
     * שליפת משאב מהמערכת וביצוע המרה לטיפוס המבוקש.
     * @tparam T סוג המשאב הקונקרטי אליו אנו רוצים להמיר.
     * @param assetId המזהה הטקסטואלי של המשאב.
     */
    template <typename T>
    T& getAsset(std::string_view assetId) const {
        static_assert(std::is_base_of_v<IAsset, T>, "T must derive from IAsset");
        
        auto it = m_assets.find(std::string(assetId));
        if (it == m_assets.end()) {
            throw std::runtime_error("Asset not found: " + std::string(assetId));
        }

        auto* casted = dynamic_cast<T*>(it->second.get());
        if (!casted) {
            throw std::runtime_error("Invalid asset type cast for: " + std::string(assetId));
        }

        return *casted;
    }

    // Remove a specific resource from memory
    void unloadAsset(std::string_view assetId) {
        m_assets.erase(std::string(assetId));
    }

    // Completely clear all loaded resources
    void clear() {
        m_assets.clear();
    }
};
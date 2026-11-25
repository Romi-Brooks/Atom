#ifndef ATOM_HOMELESS_PROMENADECONFIG_HPP
#define ATOM_HOMELESS_PROMENADECONFIG_HPP

#include <string_view>

namespace homeless {

inline constexpr int scene_width = 560;
inline constexpr int scene_height = 315;
inline constexpr float walk_boundary = 18.0f;
inline constexpr float walker_feet = 228.0f;
inline constexpr int walker_layer = 10;
inline constexpr int pedestrian_layer = 9;
inline constexpr std::string_view waterfront_uri = "res://Textures/WaterfrontPanorama.png";
inline constexpr std::string_view street_uri = "res://Textures/BridgeStreet.png";
inline constexpr std::string_view walker_uri = "res://Textures/Walker.png";
inline constexpr std::string_view pedestrian_uri = "res://Textures/Pedestrians.png";
inline constexpr std::string_view car_uri = "res://Textures/TrafficFleet.png";
inline constexpr std::string_view snow_uri = "res://Textures/Snow.png";
inline constexpr std::string_view breath_uri = "res://Textures/Breath.png";

struct StreetEntrance {
        float x;
        std::string_view name;
        std::string_view fallback_name;
};

} // namespace homeless

#endif // ATOM_HOMELESS_PROMENADECONFIG_HPP

#ifndef ATOM_HOMELESS_PROMENADECONFIG_HPP
#define ATOM_HOMELESS_PROMENADECONFIG_HPP

#include <string_view>

namespace homeless {

inline constexpr int scene_width = 560;
inline constexpr int scene_height = 315;
inline constexpr std::string_view scene_uri = "res://Textures/WaterfrontPanorama.png";
inline constexpr std::string_view walker_uri = "res://Textures/Walker.png";
inline constexpr std::string_view car_uri = "res://Textures/TrafficFleet.png";
inline constexpr std::string_view snow_uri = "res://Textures/Snow.png";
inline constexpr std::string_view breath_uri = "res://Textures/Breath.png";

} // namespace homeless

#endif // ATOM_HOMELESS_PROMENADECONFIG_HPP

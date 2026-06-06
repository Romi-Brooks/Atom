/**
	* @file           : Vec2.hpp
	* @author         : Romi Brooks
	* @brief          : Vector 2d for Atom Engine
	* @attention      :
	* @date           : 2026/6/6
	Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_VEC2_HPP
#define ATOM_VEC2_HPP

namespace atom {
	class Vec2 {
		private:
			float x_ = 0;
			float y_ = 0;

		public:
			Vec2() = default;
			Vec2(const float x, const float y) : x_(x), y_(y) {}

			[[nodiscard]] auto GetX() const -> float;
			[[nodiscard]] auto GetY() const -> float;

			auto SetX(float x) -> void;
			auto SetY(float y) -> void;
	};

} // namespace atom

#endif // ATOM_VEC2_HPP

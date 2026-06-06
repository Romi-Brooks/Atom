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
			int x_ = 0;
			int y_ = 0;

		public:
			Vec2() = default;
			Vec2(const int x, const int y) : x_(x), y_(y) {}

			[[nodiscard]] auto GetX() const -> int { return x_; }
			[[nodiscard]] auto GetY() const -> int { return y_; }

			auto SetX(const int x) -> void { x_ = x; }
			auto SetY(const int y) -> void { y_ = y; }
	};

} // namespace atom

#endif // ATOM_VEC2_HPP

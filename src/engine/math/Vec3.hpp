#pragma once

#include <cmath>

namespace isoweb {
namespace engine {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vec3() = default;
  Vec3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}

  Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
  Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
  Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
  Vec3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }
};

inline float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

inline float length(const Vec3& value) {
  return std::sqrt(dot(value, value));
}

inline Vec3 normalise(const Vec3& value) {
  const float magnitude = length(value);
  return magnitude > 0.0f ? value / magnitude : Vec3();
}

inline Vec3 rotateZ(const Vec3& value, float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {
    value.x * cosine - value.y * sine,
    value.x * sine + value.y * cosine,
    value.z
  };
}

} // namespace engine
} // namespace isoweb

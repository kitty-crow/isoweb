#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "engine/world/Character.hpp"
#include "engine/world/World.hpp"

namespace isoweb {
namespace demo {

class DemoObstacleSystem {
public:
  explicit DemoObstacleSystem(engine::World& world);

  bool enabled() const { return enabled_; }
  void setEnabled(bool enabled);
  std::vector<std::string> tick(float deltaSeconds);

  bool isObstacleCharacter(const engine::Character& character) const;
  std::size_t partCount() const { return partIds_.size(); }

private:
  struct LocalBounds {
    engine::Vec3 minimum;
    engine::Vec3 maximum;
  };

  engine::World& world_;
  bool enabled_ = false;
  float barrierPhase_ = 0.0f;
  float guillotinePhase_ = 0.0f;
  float bladePhase_ = 0.0f;
  std::vector<std::string> partIds_;
  std::vector<std::string> navigationIds_;

  void spawn();
  void remove();
  void updateBarrier(float deltaSeconds);
  void updateGuillotine(float deltaSeconds);
  void updateBlades(float deltaSeconds);

  engine::Character* part(const std::string& id);
  const engine::Character* part(const std::string& id) const;

  static LocalBounds boundsInLocalSpace(
    const engine::Object& reference,
    const engine::Object& other
  );
  static bool touchesFace(
    const engine::Object& obstacle,
    const engine::Object& other,
    engine::ObjectFace face,
    float tolerance
  );
  static bool touchesAnyFace(
    const engine::Object& obstacle,
    const engine::Object& other,
    float tolerance
  );

  std::vector<std::string> hazardContacts() const;
};

} // namespace demo
} // namespace isoweb

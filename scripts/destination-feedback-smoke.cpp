#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"

using isoweb::engine::Camera;
using isoweb::engine::CameraConfig;
using isoweb::engine::Character;
using isoweb::engine::CharacterSystem;
using isoweb::engine::EntityLocation;
using isoweb::engine::Ray;
using isoweb::engine::Vec3;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[destination-feedback] " << message << '\n';
    std::exit(1);
  }
}

float horizontalLength(const Vec3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec3 normalisedHorizontal(const Vec3& value) {
  const float length = horizontalLength(value);
  if (length < 1e-6f) return {0.0f, 1.0f, 0.0f};
  return {value.x / length, value.y / length, 0.0f};
}

float colourDistance(const Vec3& a, const Vec3& b) {
  const float x = a.x - b.x;
  const float y = a.y - b.y;
  const float z = a.z - b.z;
  return std::sqrt(x * x + y * y + z * z);
}

struct SamplePair {
  Vec3 environment;
  Vec3 runtime;
};

SamplePair sampleFloor(isoweb::demo::DemoWorld& world, const Vec3& point) {
  const Ray ray{{point.x, point.y, 5.0f}, {0.0f, 0.0f, -1.0f}};
  float environmentDistance = 0.0f;
  SamplePair pair;
  pair.environment = world.sampleEnvironment(ray, 0.5f, environmentDistance);
  pair.runtime = world.compositeRuntime(ray, pair.environment, environmentDistance);
  return pair;
}

} // namespace

int main() {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = "destination-feedback-runner";
  character->location = {"demo", "default", "middle", {0.0f, 2.40f, 0.0f}};
  // Deliberately rectangular so orientation is observable in the projected base.
  character->hitBox.minimum = {-0.38f, -0.16f, 0.0f};
  character->hitBox.maximum = {0.38f, 0.16f, 1.65f};
  character->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));

  characters.selection().style.tint = {0.92f, 0.18f, 0.86f};
  characters.selection().style.strength = 0.72f;

  EntityLocation destination = character->location;
  destination.position = {3.0f, 3.0f, 0.0f};
  require(characters.command(*character, destination), "safe floor destination was not commandable");
  require(character->moving && character->movement.hasDestination, "movement state did not arm feedback");
  require(horizontalLength(character->movement.destinationForward) > 0.99f, "final route facing was not cached");

  const Vec3 finalForward = normalisedHorizontal(character->movement.destinationForward);
  const Vec3 finalRight{finalForward.y, -finalForward.x, 0.0f};
  const Vec3 centre = character->movement.destination.position;

  world.prepareRenderFrame({0.0f, 0.0f, -1.0f});
  const SamplePair centreAtReceipt = sampleFloor(world, centre);
  require(
    colourDistance(centreAtReceipt.environment, centreAtReceipt.runtime) > 0.03f,
    "destination centre did not receive translucent selection-colour feedback"
  );

  // The Character's wider local X/base-right dimension must rotate with the
  // final route facing. This proves we are projecting the actual oriented base,
  // not drawing an axis-aligned screen marker.
  const Vec3 insideWideAxis = centre + finalRight * 0.28f;
  const Vec3 outsideNarrowAxis = centre + finalForward * 0.28f;
  const SamplePair wideAxis = sampleFloor(world, insideWideAxis);
  const SamplePair narrowAxis = sampleFloor(world, outsideNarrowAxis);
  require(
    colourDistance(wideAxis.environment, wideAxis.runtime) > 0.02f,
    "oriented wide side of destination footprint was not highlighted"
  );
  require(
    colourDistance(narrowAxis.environment, narrowAxis.runtime) < 0.005f,
    "destination footprint ignored Character base orientation"
  );

  // Same tint, stronger front strip: this disambiguates final forward from a
  // 180-degree-reversed rectangle while keeping the whole marker translucent.
  const SamplePair front = sampleFloor(world, centre + finalForward * 0.13f);
  const SamplePair back = sampleFloor(world, centre - finalForward * 0.13f);
  require(
    colourDistance(front.environment, front.runtime) >
      colourDistance(back.environment, back.runtime) + 0.015f,
    "destination footprint did not communicate which edge is the final front"
  );

  // The feedback must actually flash while movement is active. A quarter pulse
  // period takes the initial midpoint pulse to its bright peak.
  characters.tick(1.0f / 7.0f, camera);
  require(character->moving, "Character arrived before pulse regression could run");
  world.prepareRenderFrame({0.0f, 0.0f, -1.0f});
  const SamplePair centreAtPeak = sampleFloor(world, centre);
  require(
    colourDistance(centreAtReceipt.runtime, centreAtPeak.runtime) > 0.02f,
    "destination acknowledgement did not pulse over time"
  );

  // Cancelling motion must remove the acknowledgement immediately, without a
  // static-scene change or another environment trace requirement.
  characters.stop(*character);
  world.prepareRenderFrame({0.0f, 0.0f, -1.0f});
  const SamplePair afterStop = sampleFloor(world, centre);
  require(
    colourDistance(afterStop.environment, afterStop.runtime) < 0.005f,
    "destination feedback remained after movement stopped"
  );

  // Lower preview runtime entities remain dynamic without rendering a second
  // complete level. From the upper level, choose rays that miss upper and
  // middle geometry before reaching exposed points on the lower west room.
  isoweb::demo::DemoWorld previewWorld;
  CharacterSystem previewCharacters(previewWorld);
  require(previewWorld.levelUp(), "could not switch preview regression to upper level");

  std::unique_ptr<Character> lowerOwned(new Character());
  Character* lowerCharacter = lowerOwned.get();
  lowerCharacter->id = "lower-preview-runner";
  lowerCharacter->location = {"demo", "default", "lower", {-12.0f, 0.0f, 0.0f}};
  lowerCharacter->hitBox.minimum = {-0.38f, -0.16f, 0.0f};
  lowerCharacter->hitBox.maximum = {0.38f, 0.16f, 1.65f};
  lowerCharacter->forward = {0.0f, 1.0f, 0.0f};
  lowerCharacter->moving = true;
  lowerCharacter->movement.hasDestination = true;
  lowerCharacter->movement.destination = lowerCharacter->location;
  // Keep the floor-bound marker far enough inside the west room that the
  // taller near cutaway wall does not legitimately occlude it from this ray.
  lowerCharacter->movement.destination.position = {-11.0f, -2.0f, 0.0f};
  lowerCharacter->movement.destinationForward = {0.0f, -1.0f, 0.0f};
  lowerCharacter->movement.feedbackElapsedSeconds = 0.08f;
  previewWorld.entities().add(std::move(lowerOwned));

  previewCharacters.selection().style.tint = {0.92f, 0.18f, 0.86f};
  previewCharacters.selection().style.strength = 0.72f;

  const Vec3 previewDirection = normalisedHorizontal({1.0f, 1.0f, 0.0f}) * 0.81649658f +
    Vec3(0.0f, 0.0f, -0.57735027f);
  previewWorld.prepareRenderFrame(previewDirection);

  const float lowerPreviewZ = previewWorld.levelViewOrigin("lower").z -
    previewWorld.levelViewOrigin("upper").z;
  auto cheapPreview = [&](const Vec3& floorPoint, Vec3& colour) {
    const Ray ray{floorPoint - previewDirection * 24.0f, previewDirection};
    return previewWorld.sampleLowDetailLowerPreview(ray, colour);
  };

  Vec3 lowerBody;
  require(
    cheapPreview({-12.0f, 0.0f, lowerPreviewZ}, lowerBody),
    "second lower preview floor was not available to the cheap sampler"
  );
  require(
    colourDistance(lowerBody, {0.82f, 0.84f, 0.88f}) < 0.05f,
    "Character on second lower preview level was not visible in the cheap preview"
  );

  Vec3 withMarker;
  require(
    cheapPreview({-11.0f, -2.0f, lowerPreviewZ}, withMarker),
    "lower destination preview floor was not available"
  );
  lowerCharacter->moving = false;
  previewWorld.prepareRenderFrame(previewDirection);
  Vec3 withoutMarker;
  require(cheapPreview({-11.0f, -2.0f, lowerPreviewZ}, withoutMarker), "marker baseline preview missing");
  require(
    colourDistance(withMarker, withoutMarker) > 0.02f,
    "destination feedback on second lower preview level was not visible in the cheap preview"
  );

  std::cout << "Character destination acknowledgement and lower-preview runtime smoke test passed.\n";
  return 0;
}

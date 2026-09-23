#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/render/GpuStaticScene.hpp"
#include "engine/render/Ray.hpp"
#include "engine/world/SceneSurface.hpp"
#include "engine/world/WorldObject.hpp"

namespace isoweb {
namespace engine {

struct WorldBounds {
  Vec3 focus;
  std::vector<Vec3> points;
};

class IWorld {
public:
  virtual ~IWorld() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual const WorldBounds& cameraBounds() const { return bounds(); }
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
  virtual bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
  virtual bool collidesWith(const Object& candidate) const = 0;

  // Split static environment sampling from runtime-entity compositing so a
  // renderer may cache exact static supersamples while the camera/level stay
  // unchanged. Defaults preserve compatibility for worlds that do not opt in.
  virtual bool supportsStaticSampleCache() const { return false; }

  // Optional compact static scene for GPU ray/shadow evaluation. Worlds that
  // cannot faithfully describe the active level return false and the software
  // renderer remains authoritative.
  virtual bool buildGpuStaticScene(GpuStaticScene&) const { return false; }

  // Lower levels are decorative previews, not additional fully rendered scenes.
  // Implementations may expose a cheap analytic preview sampler which the
  // renderer evaluates at reduced resolution. This must not perform scene
  // ray-tracing, shadow queries, or other active-level-quality work.
  virtual bool supportsLowDetailLowerPreview() const { return false; }
  virtual float lowerPreviewResolutionScale() const { return 0.25f; }
  virtual std::uint64_t lowDetailPreviewRevision() const { return 0; }
  virtual bool sampleLowDetailLowerPreview(const Ray&, Vec3&) const { return false; }

  virtual Vec3 sampleEnvironment(
    const Ray& ray,
    float backgroundY,
    float& environmentDistance
  ) const {
    environmentDistance = std::numeric_limits<float>::max();
    return sample(ray, backgroundY);
  }

  virtual Vec3 compositeRuntime(
    const Ray&,
    const Vec3& environmentColour,
    float
  ) const {
    return environmentColour;
  }

  // Threaded renderers may assign a stable worker slot so a world can keep
  // reusable per-worker scratch without TLS lookups or per-ray construction.
  // Worlds that do not need scratch simply delegate to the normal compositor.
  virtual Vec3 compositeRuntimeForWorker(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    std::size_t
  ) const {
    return compositeRuntime(ray, environmentColour, environmentDistance);
  }

  // Parallel frame compositing is opt-in. Worlds returning true promise that
  // compositeRuntime() and any lazy static sample refill are safe when called
  // concurrently for disjoint pixels after prepareRenderFrame().
  virtual bool runtimeCompositeThreadSafe() const { return false; }

  // Called once immediately before a render pass. Worlds can use this to cache
  // frame-invariant entity projection/presentation state out of the ray loop.
  virtual void prepareRenderFrame(const Vec3&) const {}

  virtual std::size_t levelCount() const = 0;
  virtual std::size_t activeLevelIndex() const = 0;
  virtual std::size_t defaultLevelIndex() const = 0;
};

} // namespace engine
} // namespace isoweb

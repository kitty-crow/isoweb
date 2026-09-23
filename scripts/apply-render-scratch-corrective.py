from pathlib import Path

hpp = Path("src/engine/world/World.hpp")
text = hpp.read_text()
old = '''template <typename T>
class ThreadLocalVector {
public:
  std::vector<T>& get() const {
    thread_local std::vector<T> value;
    return value;
  }

  operator std::vector<T>&() const { return get(); }
  void clear() const { get().clear(); }
  std::size_t capacity() const { return get().capacity(); }
  void reserve(std::size_t size) const { get().reserve(size); }
};

'''
if text.count(old) != 1:
    raise SystemExit(f"ThreadLocalVector declaration count={text.count(old)}")
text = text.replace(old, "", 1)
old = '''  mutable ThreadLocalVector<RuntimeSample> runtimeSampleScratch_;
'''
if text.count(old) != 1:
    raise SystemExit(f"runtimeSampleScratch member count={text.count(old)}")
text = text.replace(old, "", 1)
hpp.write_text(text)

cpp = Path("src/engine/world/World.cpp")
text = cpp.read_text()
old = '''  // Lower-preview geometry is static. Characters and destination feedback are
  // composited separately at full screen resolution, so their motion does not
  // invalidate or restart progressive floor refinement.
  runtimeSampleScratch_.clear();
  if (runtimeSampleScratch_.capacity() < runtimeRenderEntries_.size()) {
    runtimeSampleScratch_.reserve(runtimeRenderEntries_.size());
  }
  runtimeRenderCachePrepared_ = true;
'''
new = '''  // Lower-preview geometry is static. Characters and destination feedback are
  // composited separately at full screen resolution, so their motion does not
  // invalidate or restart progressive floor refinement.
  runtimeRenderCachePrepared_ = true;
'''
if text.count(old) != 1:
    raise SystemExit(f"prepare scratch block count={text.count(old)}")
text = text.replace(old, new, 1)

old = '''  std::vector<RuntimeSample>& samples = runtimeSampleScratch_;
  samples.clear();

  const float rayDirectionLengthSquared = dot(ray.direction, ray.direction);
'''
new = '''  // Runtime samples are normally extremely shallow. Keep the common path
  // entirely on the calling thread's stack so parallel rows need no shared or
  // thread-local scratch lookup. The vector is only populated if more than
  // eight dynamic surfaces overlap one ray, preserving unlimited exact
  // compositing for pathological scenes without taxing ordinary frames.
  std::array<RuntimeSample, 8> localSamples;
  std::size_t localSampleCount = 0;
  std::vector<RuntimeSample> overflowSamples;
  const auto appendSample = [&](const RuntimeSample& value) {
    if (overflowSamples.empty() && localSampleCount < localSamples.size()) {
      localSamples[localSampleCount++] = value;
      return;
    }
    if (overflowSamples.empty()) {
      overflowSamples.reserve(std::max<std::size_t>(
        runtimeRenderEntries_.size(),
        localSamples.size() + 1
      ));
      overflowSamples.insert(
        overflowSamples.end(),
        localSamples.begin(),
        localSamples.begin() + static_cast<std::ptrdiff_t>(localSampleCount)
      );
    }
    overflowSamples.push_back(value);
  };

  const float rayDirectionLengthSquared = dot(ray.direction, ray.direction);
'''
if text.count(old) != 1:
    raise SystemExit(f"sample scratch opening count={text.count(old)}")
text = text.replace(old, new, 1)

push_count = text.count("            samples.push_back(sample);")
if push_count != 1:
    raise SystemExit(f"sprite samples.push_back count={push_count}")
text = text.replace("            samples.push_back(sample);", "            appendSample(sample);", 1)
push_count = text.count("    samples.push_back(sample);")
if push_count != 1:
    raise SystemExit(f"proxy samples.push_back count={push_count}")
text = text.replace("    samples.push_back(sample);", "    appendSample(sample);", 1)

old = '''  if (samples.empty()) {
    found = destinationFound || previewDestinationFound;
    return found ? compositedEnvironment : Vec3();
  }

  if (samples.size() > 1) {
    std::sort(samples.begin(), samples.end(), [](const RuntimeSample& a, const RuntimeSample& b) {
      return a.distance > b.distance;
    });
  }

  Vec3 colour = compositedEnvironment;
  for (const RuntimeSample& sample : samples) {
    const float alpha = std::max(0.0f, std::min(1.0f, sample.alpha));
    colour = sample.colour * alpha + colour * (1.0f - alpha);
  }
'''
new = '''  if (localSampleCount == 0 && overflowSamples.empty()) {
    found = destinationFound || previewDestinationFound;
    return found ? compositedEnvironment : Vec3();
  }

  const auto fartherFirst = [](const RuntimeSample& a, const RuntimeSample& b) {
    return a.distance > b.distance;
  };
  if (!overflowSamples.empty()) {
    if (overflowSamples.size() > 1) {
      std::sort(overflowSamples.begin(), overflowSamples.end(), fartherFirst);
    }
  } else if (localSampleCount > 1) {
    std::sort(
      localSamples.begin(),
      localSamples.begin() + static_cast<std::ptrdiff_t>(localSampleCount),
      fartherFirst
    );
  }

  Vec3 colour = compositedEnvironment;
  const auto compositeSample = [&](const RuntimeSample& sample) {
    const float alpha = std::max(0.0f, std::min(1.0f, sample.alpha));
    colour = sample.colour * alpha + colour * (1.0f - alpha);
  };
  if (!overflowSamples.empty()) {
    for (const RuntimeSample& sample : overflowSamples) compositeSample(sample);
  } else {
    for (std::size_t index = 0; index < localSampleCount; ++index) {
      compositeSample(localSamples[index]);
    }
  }
'''
if text.count(old) != 1:
    raise SystemExit(f"sample scratch closing count={text.count(old)}")
text = text.replace(old, new, 1)
cpp.write_text(text)

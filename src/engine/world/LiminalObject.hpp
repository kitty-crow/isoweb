#pragma once

#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

// A liminal object is a piece of world geometry whose job is to connect two
// spatial contexts. It belongs to neither endpoint level/world exclusively.
// The two traversal arrays describe the same physical connector from each
// endpoint's local coordinate frame:
//   - forwardTraversal is ordered from fromLevelId -> toLevelId
//   - reverseTraversal is ordered from toLevelId -> fromLevelId
// Corresponding physical samples are therefore forward[i] and
// reverse[reverse.size() - 1 - i].
struct LiminalObject {
  std::string id;
  std::string category = "liminal";
  std::string type = "connector";

  std::string fromWorldId;
  std::string toWorldId;
  std::string fromLevelId;
  std::string toLevelId;
  Vec3 fromPosition;
  Vec3 toPosition;
  std::vector<Vec3> forwardTraversal;
  std::vector<Vec3> reverseTraversal;
  bool bidirectional = true;

  // Cached coordinate-frame transform for the current engine's translated
  // endpoint frames. setNavigationLinks derives this from paired traversal
  // samples, so rendering the opposite endpoint never requires loading it.
  Vec3 fromToViewOffset;
  bool hasViewOffset = false;

  bool touchesLevel(const std::string& levelId) const {
    return levelId == fromLevelId || levelId == toLevelId;
  }
};

// Compatibility name retained for the navigation subsystem. Navigation links
// are now explicitly the liminal objects that connect spaces.
using NavigationLink = LiminalObject;

} // namespace engine
} // namespace isoweb

// Incremental renderer front-end for the existing isoweb renderer.
//
// main.cpp still contains the scene/intersection/control implementation. This
// translation unit includes it with the old exported entry points renamed, so
// the public WASM API can drive the same renderer incrementally instead of
// blocking the browser for an entire frame.

#define traceSample traceSampleLegacy
#define isoweb_render isoweb_render_legacy
#define isoweb_resize isoweb_resize_legacy
#define isoweb_rotate_clockwise isoweb_rotate_clockwise_legacy
#define isoweb_rotate_counterclockwise isoweb_rotate_counterclockwise_legacy
#define isoweb_reset_yaw isoweb_reset_yaw_legacy
#define isoweb_zoom_in isoweb_zoom_in_legacy
#define isoweb_zoom_out isoweb_zoom_out_legacy
#define isoweb_reset_zoom isoweb_reset_zoom_legacy
#define isoweb_set_detailed_mode isoweb_set_detailed_mode_legacy
#define isoweb_pan isoweb_pan_legacy
#define isoweb_reset_camera isoweb_reset_camera_legacy
#define isoweb_level_up isoweb_level_up_legacy
#define isoweb_level_down isoweb_level_down_legacy
#define isoweb_reset_level isoweb_reset_level_legacy
#include "main.cpp"
#undef traceSample
#undef isoweb_render
#undef isoweb_resize
#undef isoweb_rotate_clockwise
#undef isoweb_rotate_counterclockwise
#undef isoweb_reset_yaw
#undef isoweb_zoom_in
#undef isoweb_zoom_out
#undef isoweb_reset_zoom
#undef isoweb_set_detailed_mode
#undef isoweb_pan
#undef isoweb_reset_camera
#undef isoweb_level_up
#undef isoweb_level_down
#undef isoweb_reset_level

namespace {

constexpr int DEMO_LEVEL_COUNT = 3;
constexpr int PROGRESS_HEIGHT = 22;
constexpr int PROGRESS_TOP = 18;
constexpr int PROGRESS_MIN_WIDTH = 96;
constexpr int PROGRESS_MAX_WIDTH = 220;
constexpr float GHOST_OBJECT_ALPHA = 0.24f;
constexpr float GHOST_FLOOR_ALPHA = 0.055f;

bool renderActive = false;
bool renderTickScheduled = false;
int nextRenderRow = 0;

LevelView makeDemoLevel(int index) {
    LevelView level;

    if (index == 0) {
        level.lightPosition = Vec3(4.20f, -3.20f, 5.60f);
        level.floorDark = Vec3(0.34f, 0.34f, 0.36f);
        level.floorLight = Vec3(0.40f, 0.40f, 0.42f);
        level.objects.push_back({ShapeKind::Cone, Vec3(-1.30f, -0.80f, 0.0f), 0.86f, Vec3(0.62f, 0.25f, 0.82f)});
        level.objects.push_back({ShapeKind::Pyramid, Vec3(1.20f, 0.85f, 0.0f), 0.92f, Vec3(0.96f, 0.78f, 0.16f)});
        return level;
    }

    if (index == 1) {
        level.lightPosition = Vec3(-3.60f, -4.20f, 6.50f);
        level.floorDark = Vec3(0.567f, 0.605f, 0.630f);
        level.floorLight = Vec3(0.621f, 0.662f, 0.690f);
        level.objects.push_back({ShapeKind::Cube, Vec3(-1.05f, 0.65f, 0.80f), 0.80f, Vec3(0.18f, 0.48f, 0.88f)});
        level.objects.push_back({ShapeKind::Sphere, Vec3(1.05f, -0.25f, 0.90f), 0.90f, Vec3(0.95f, 0.43f, 0.12f)});
        return level;
    }

    level.lightPosition = Vec3(3.80f, 4.40f, 7.20f);
    level.floorDark = Vec3(0.74f, 0.74f, 0.76f);
    level.floorLight = Vec3(0.82f, 0.82f, 0.84f);
    level.objects.push_back({ShapeKind::Dodecahedron, Vec3(-1.35f, 0.95f, 1.00f), 0.72f, Vec3(0.18f, 0.50f, 0.94f)});
    level.objects.push_back({ShapeKind::Icosahedron, Vec3(1.30f, -0.95f, 1.10f), 0.78f, Vec3(0.90f, 0.16f, 0.14f)});
    return level;
}

void unloadLevel(LevelView& level) {
    level.objects = std::vector<RenderObject>();
    level.lightPosition = Vec3();
    level.floorDark = Vec3();
    level.floorLight = Vec3();
}

void materialiseVisibleLevels() {
    if (levels.size() != DEMO_LEVEL_COUNT) levels.resize(DEMO_LEVEL_COUNT);

    for (int index = 0; index < DEMO_LEVEL_COUNT; ++index) {
        if (index != selectedLevel && index != selectedLevel - 1) {
            unloadLevel(levels[static_cast<std::size_t>(index)]);
        }
    }

    levels[static_cast<std::size_t>(selectedLevel)] = makeDemoLevel(selectedLevel);
    if (selectedLevel > 0) {
        levels[static_cast<std::size_t>(selectedLevel - 1)] = makeDemoLevel(selectedLevel - 1);
    }
}

// Active level receives the normal lighting/shadow path. Only the level
// directly below the active one can appear as a ghost, and that ghost is
// intentionally flat-colour: no lighting calculation and no shadow rays.
Vec3 traceSample(float pixelX, float pixelY) {
    const Ray ray = cameraRay(pixelX, pixelY);
    const LevelView& activeLevel = levels[static_cast<std::size_t>(selectedLevel)];
    const Hit activeHit = traceClosest(activeLevel, ray, EPSILON, FAR_DISTANCE);
    Vec3 colour = activeHit.found
        ? shade(activeLevel, activeHit)
        : backgroundColour(pixelY / static_cast<float>(frameHeight));

    if (selectedLevel > 0) {
        const LevelView& lowerLevel = levels[static_cast<std::size_t>(selectedLevel - 1)];
        const Hit ghostHit = traceClosest(lowerLevel, ray, EPSILON, FAR_DISTANCE);
        if (ghostHit.found) {
            const float alpha = ghostHit.ground ? GHOST_FLOOR_ALPHA : GHOST_OBJECT_ALPHA;
            colour = blend(colour, ghostHit.colour, alpha);
        }
    }

    return colour;
}

void writeRgbaPixel(int x, int y, const Vec3& colour) {
    const std::size_t index = static_cast<std::size_t>((y * frameWidth + x) * 4);
    rgba[index] = toByte(colour.x);
    rgba[index + 1] = toByte(colour.y);
    rgba[index + 2] = toByte(colour.z);
    rgba[index + 3] = 255;
}

void fillRenderSurface() {
    const Vec3 background(0.075f, 0.12f, 0.18f);
    const dsr::ColorRgbaI32 pixel(toByte(background.x), toByte(background.y), toByte(background.z), 255);
    dsr::image_fill(frame, pixel);

    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            writeRgbaPixel(x, y, background);
        }
    }
}

int rowsPerChunk() {
    // Keep each synchronous chunk small enough that mobile Safari gets regular
    // opportunities to repaint. Wider framebuffers therefore use fewer rows.
    return std::max(1, std::min(8, 1500 / std::max(frameWidth, 1)));
}

void renderRows(int startRow, int endRow) {
    const float offsets[2] = {0.25f, 0.75f};

    for (int y = startRow; y < endRow; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            Vec3 colour;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    colour = colour + traceSample(x + offsets[sx], y + offsets[sy]);
                }
            }
            colour = colour * 0.25f;
            dsr::image_writePixel(
                frame,
                x,
                y,
                dsr::ColorRgbaI32(toByte(colour.x), toByte(colour.y), toByte(colour.z), 255)
            );
            writeRgbaPixel(x, y, colour);
        }
    }
}

struct ProgressRect {
    int x;
    int y;
    int width;
    int height;
};

ProgressRect progressRect() {
    const int available = std::max(PROGRESS_MIN_WIDTH, frameWidth - 72);
    const int width = std::min(PROGRESS_MAX_WIDTH, available);
    return ProgressRect{
        std::max(0, (frameWidth - width) / 2),
        std::min(PROGRESS_TOP, std::max(0, frameHeight - PROGRESS_HEIGHT)),
        width,
        PROGRESS_HEIGHT
    };
}

bool insideRoundedRect(int x, int y, int width, int height, int radius) {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    if (x >= radius && x < width - radius) return true;
    if (y >= radius && y < height - radius) return true;

    const int centreX = x < radius ? radius : width - radius - 1;
    const int centreY = y < radius ? radius : height - radius - 1;
    const int dx = x - centreX;
    const int dy = y - centreY;
    return dx * dx + dy * dy <= radius * radius;
}

std::vector<std::uint8_t> progressBackup;

void drawProgressSprite(float progress, ProgressRect& rect) {
    rect = progressRect();
    const std::size_t bytes = static_cast<std::size_t>(rect.width * rect.height * 4);
    progressBackup.resize(bytes);

    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const std::size_t source = static_cast<std::size_t>(((rect.y + y) * frameWidth + rect.x + x) * 4);
            const std::size_t local = static_cast<std::size_t>((y * rect.width + x) * 4);
            progressBackup[local] = rgba[source];
            progressBackup[local + 1] = rgba[source + 1];
            progressBackup[local + 2] = rgba[source + 2];
            progressBackup[local + 3] = rgba[source + 3];
        }
    }

    const int radius = 7;
    const int border = 2;
    const int innerX = 6;
    const int innerY = 7;
    const int innerWidth = std::max(1, rect.width - innerX * 2);
    const int innerHeight = std::max(1, rect.height - innerY * 2);
    const int filled = static_cast<int>(std::round(innerWidth * std::max(0.0f, std::min(1.0f, progress))));

    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            if (!insideRoundedRect(x, y, rect.width, rect.height, radius)) continue;

            const bool edge =
                !insideRoundedRect(x - border, y - border, rect.width - border * 2, rect.height - border * 2, radius - border);
            const bool inTrack =
                x >= innerX && x < innerX + innerWidth &&
                y >= innerY && y < innerY + innerHeight;
            const bool inFill = inTrack && x < innerX + filled;

            int red = 14;
            int green = 19;
            int blue = 25;
            int alpha = 226;

            if (edge) {
                red = green = blue = 126;
                alpha = 215;
            } else if (inFill) {
                red = green = blue = 238;
                alpha = 246;
            } else if (inTrack) {
                red = green = blue = 67;
                alpha = 235;
            }

            const std::size_t destination = static_cast<std::size_t>(((rect.y + y) * frameWidth + rect.x + x) * 4);
            const float a = static_cast<float>(alpha) / 255.0f;
            rgba[destination] = static_cast<std::uint8_t>(rgba[destination] * (1.0f - a) + red * a);
            rgba[destination + 1] = static_cast<std::uint8_t>(rgba[destination + 1] * (1.0f - a) + green * a);
            rgba[destination + 2] = static_cast<std::uint8_t>(rgba[destination + 2] * (1.0f - a) + blue * a);
            rgba[destination + 3] = 255;
        }
    }
}

void restoreProgressSprite(const ProgressRect& rect) {
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const std::size_t destination = static_cast<std::size_t>(((rect.y + y) * frameWidth + rect.x + x) * 4);
            const std::size_t local = static_cast<std::size_t>((y * rect.width + x) * 4);
            rgba[destination] = progressBackup[local];
            rgba[destination + 1] = progressBackup[local + 1];
            rgba[destination + 2] = progressBackup[local + 2];
            rgba[destination + 3] = progressBackup[local + 3];
        }
    }
}

void presentPartialRows(int startRow, int rowCount, float progress) {
    ProgressRect rect;
    drawProgressSprite(progress, rect);
    const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(rgba.data());

    EM_ASM({
        const pointer = $0;
        const width = $1;
        const height = $2;
        const dirtyY = $3;
        const dirtyHeight = $4;
        const progressX = $5;
        const progressY = $6;
        const progressWidth = $7;
        const progressHeight = $8;

        const canvas = document.getElementById('canvas');
        if (!canvas) return;
        if (canvas.width !== width) canvas.width = width;
        if (canvas.height !== height) canvas.height = height;
        const context = canvas.getContext('2d', { alpha: false });

        let imageData = window.__isowebProgressImageData;
        if (!imageData || imageData.width !== width || imageData.height !== height) {
            imageData = context.createImageData(width, height);
            window.__isowebProgressImageData = imageData;
        }

        const copyRect = function(x, y, w, h) {
            const clampedX = Math.max(0, Math.min(width, x));
            const clampedY = Math.max(0, Math.min(height, y));
            const clampedW = Math.max(0, Math.min(width - clampedX, w));
            const clampedH = Math.max(0, Math.min(height - clampedY, h));
            if (!clampedW || !clampedH) return;
            for (let row = 0; row < clampedH; ++row) {
                const pixelOffset = (clampedY + row) * width + clampedX;
                const byteOffset = pixelOffset * 4;
                const byteCount = clampedW * 4;
                imageData.data.set(
                    HEAPU8.subarray(pointer + byteOffset, pointer + byteOffset + byteCount),
                    byteOffset
                );
            }
            context.putImageData(imageData, 0, 0, clampedX, clampedY, clampedW, clampedH);
        };

        copyRect(0, dirtyY, width, dirtyHeight);
        copyRect(progressX, progressY, progressWidth, progressHeight);
        document.documentElement.classList.add('wasm-ready');
        const loading = document.getElementById('loading');
        if (loading) loading.hidden = true;
    }, static_cast<int>(pointer), frameWidth, frameHeight, startRow, rowCount,
       rect.x, rect.y, rect.width, rect.height);

    restoreProgressSprite(rect);
}

void syncRgbaFromFrame() {
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            const dsr::ColorRgbaI32 colour = dsr::image_readPixel_border(frame, x, y);
            const std::size_t index = static_cast<std::size_t>((y * frameWidth + x) * 4);
            rgba[index] = static_cast<std::uint8_t>(colour.red);
            rgba[index + 1] = static_cast<std::uint8_t>(colour.green);
            rgba[index + 2] = static_cast<std::uint8_t>(colour.blue);
            rgba[index + 3] = 255;
        }
    }
}

void renderTick(void*);

void scheduleRenderTick() {
    if (renderTickScheduled) return;
    renderTickScheduled = true;
    emscripten_async_call(renderTick, nullptr, 0);
}

void beginIncrementalRender() {
    materialiseVisibleLevels();
    ensureFrame();
    fillRenderSurface();
    nextRenderRow = 0;
    renderActive = true;

    // The 0% indicator is already a C++-generated sprite. The browser only
    // blits the bytes provided by the engine.
    presentPartialRows(0, frameHeight, 0.0f);
    scheduleRenderTick();
}

void renderTick(void*) {
    renderTickScheduled = false;
    if (!renderActive) return;

    const int start = nextRenderRow;
    const int end = std::min(frameHeight, start + rowsPerChunk());
    renderRows(start, end);
    nextRenderRow = end;

    if (nextRenderRow < frameHeight) {
        presentPartialRows(start, end - start, static_cast<float>(nextRenderRow) / static_cast<float>(frameHeight));
        scheduleRenderTick();
        return;
    }

    renderControls();
    syncRgbaFromFrame();
    renderActive = false;
    presentScene();
}

bool setYawStep(int next) {
    next &= 7;
    if (cameraYawStep == next) return false;
    cameraYawStep = next;
    return true;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() {
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_resize(int width, int height) {
    const int nextWidth = std::max(160, std::min(1600, width));
    const int nextHeight = std::max(160, std::min(1600, height));
    if (dsr::image_exists(frame) && frameWidth == nextWidth && frameHeight == nextHeight) return;
    frameWidth = nextWidth;
    frameHeight = nextHeight;
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise() {
    if (setYawStep(cameraYawStep + 1)) beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise() {
    if (setYawStep(cameraYawStep + 7)) beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_yaw() {
    if (setYawStep(0)) beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_in() {
    if (!canZoomIn()) return;
    stepZoom(1);
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_out() {
    if (!canZoomOut()) return;
    stepZoom(-1);
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_zoom() {
    if (zoomPreset == 3) return;
    zoomPreset = 3;
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_mode(int enabled) {
    const bool next = enabled != 0;
    if (detailedZoomMode == next) return;
    detailedZoomMode = next;
    if (!detailedZoomMode) {
        if (zoomPreset < 2) zoomPreset = 2;
        if (zoomPreset > 4) zoomPreset = 4;
    }
    if (dsr::image_exists(frame)) beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float screenRight, float screenDown) {
    if (!canPanDelta(screenRight, screenDown)) return;
    panCamera(screenRight, screenDown);
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_camera() {
    if (!canPanAtCurrentZoom()) return;
    if (std::fabs(cameraPanX) <= 0.0001f && std::fabs(cameraPanY) <= 0.0001f) return;
    cameraPanX = 0.0f;
    cameraPanY = 0.0f;
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_up() {
    if (selectedLevel + 1 >= DEMO_LEVEL_COUNT) return;
    ++selectedLevel;
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_down() {
    if (selectedLevel <= 0) return;
    --selectedLevel;
    beginIncrementalRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_level() {
    if (selectedLevel == DEFAULT_LEVEL_INDEX) return;
    selectedLevel = DEFAULT_LEVEL_INDEX;
    beginIncrementalRender();
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "FrameView.h"

#include "NisNode.h"

#include "veyra/engine/PushCompareLine.h"  // pure C++, shared with the Windows build: the line
#include "VideoFrames.h"

#include <QMutex>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGImageNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QScreen>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <array>

using namespace veyra::portable;

namespace pushfps {
namespace {

QMutex sharedLock;
std::shared_ptr<Engine> sharedEngine;
std::shared_ptr<Pick> fixedPick;

// Uniform block, std140 (frame.vert / frame.frag).
struct Uniforms {
    float matrix[16];
    float opacity;
    float phase;
    float mode;
    float hudOn;
    float frameSize[2];
    float fieldGrid[2];
    float blockSize;
    float errLow;
    float errHigh;
    float brightness;
    float contrast;
    float saturation;
    float gamma;
    float pad0;
    float holdZone[4];   // Crosshair Hold: x, y, size of its square in picture pixels; [3] > 0.5 = held
    float subZone[4];    // Subtitle Hold: x, y, width, height of its band in picture pixels
    float subFlags[4];   // [0] > 0.5 = held
    float compare[4];    // the live comparison: [0] on, [1] .. [2] the original's part (picture u), [3] > 0.5 = draw the line
    float compareLine[4];  // the line in picture u: dark edges [0] .. [1], white core [2] .. [3]
};
static_assert(sizeof(Uniforms) == 208, "std140 layout of frame.vert/frame.frag");
static_assert(offsetof(Uniforms, compare) == 176, "vec4 alignment");
static_assert(offsetof(Uniforms, subZone) == 144, "vec4 alignment");
static_assert(offsetof(Uniforms, holdZone) == 128, "vec4 alignment");
static_assert(offsetof(Uniforms, frameSize) == 80, "vec2 alignment");

class FrameMaterial : public QSGMaterial {
public:
    FrameMaterial() { setFlag(Blending, false); }
    QSGMaterialType* type() const override { static QSGMaterialType t; return &t; }
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial* other) const override {
        // One node, one material: never batched with anything else.
        return this == other ? 0 : (this < other ? -1 : 1);
    }
    QSGTexture* textures[11] = {};
    Uniforms u{};
};

class FrameShader : public QSGMaterialShader {
public:
    FrameShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/frame.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/frame.frag.qsb"));
    }
    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override {
        auto* m = static_cast<FrameMaterial*>(newMaterial);
        QByteArray* buf = state.uniformData();
        if (buf->size() < int(sizeof(Uniforms))) return false;
        Uniforms u = m->u;
        std::memcpy(u.matrix, state.combinedMatrix().constData(), sizeof(u.matrix));
        u.opacity = state.opacity();
        std::memcpy(buf->data(), &u, sizeof(Uniforms));
        return true;
    }
    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* newMaterial, QSGMaterial*) override {
        auto* m = static_cast<FrameMaterial*>(newMaterial);
        if (binding < 1 || binding > 11) return;
        QSGTexture* t = m->textures[binding - 1];
        if (!t) return;
        t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = t;
    }
};

QSGMaterialShader* FrameMaterial::createShader(QSGRendererInterface::RenderMode) const { return new FrameShader; }

class FrameNode : public QSGGeometryNode {
public:
    FrameNode() : geometry_(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4) {
        setGeometry(&geometry_);
        setMaterial(&material_);
    }
    FrameMaterial material_;
    QSGGeometry geometry_;
};

// Motion field, two opaque texels per block: (x high, x low, 0) then (y high, y low, 0), each a 16-bit fixed
// point value (1/16 px, offset 32768). Alpha stays 255 because Qt Quick premultiplies image textures by
// alpha on upload; data in the alpha channel (an earlier layout) was destroyed by that.
QImage fieldImage(const MotionField& f) {
    QImage img(f.blocksX * 2, f.blocksY, QImage::Format_RGB32);
    auto enc = [](float v) { return int(std::clamp(std::lround((std::isfinite(v) ? v : 0.f) * 16.0f) + 32768L, 0L, 65535L)); };
    for (int j = 0; j < f.blocksY; ++j) {
        auto* row = reinterpret_cast<QRgb*>(img.scanLine(j));
        for (int i = 0; i < f.blocksX; ++i) {
            const size_t k = size_t(j) * size_t(f.blocksX) + size_t(i);
            const int x = enc(f.vx[k]), y = enc(f.vy[k]);
            row[i * 2 + 0] = qRgb(x >> 8, x & 255, 0);
            row[i * 2 + 1] = qRgb(y >> 8, y & 255, 0);
        }
    }
    return img;
}

// Wraps the engine's picture without copying; the QImage keeps the picture alive.
QImage frameImage(const std::shared_ptr<const Bgra>& b) {
    auto* keep = new std::shared_ptr<const Bgra>(b);
    return QImage(const_cast<uchar*>(b->px.data()), b->width, b->height, b->width * 4, QImage::Format_RGB32,
                  [](void* p) { delete static_cast<std::shared_ptr<const Bgra>*>(p); }, keep);
}

// Crosshair Hold: the square of one real frame as frame generation takes it. Wraps the pair's picture
// without copying; the QImage keeps the pair alive.
QImage holdImage(const std::shared_ptr<const HoldPair>& pair, bool second) {
    const Bgra& z = second ? pair->filledB : pair->filledA;
    auto* keep = new std::shared_ptr<const HoldPair>(pair);
    return QImage(const_cast<uchar*>(z.px.data()), z.width, z.height, z.width * 4, QImage::Format_RGB32,
                  [](void* p) { delete static_cast<std::shared_ptr<const HoldPair>*>(p); }, keep);
}

QImage maskImage(const Gray& g) {
    QImage img(g.width, g.height, QImage::Format_Grayscale8);
    for (int y = 0; y < g.height; ++y) std::memcpy(img.scanLine(y), g.px.data() + size_t(y) * size_t(g.width), size_t(g.width));
    return img;
}

}  // namespace

void FrameView::setEngine(std::shared_ptr<Engine> engine) {
    QMutexLocker lk(&sharedLock);
    sharedEngine = std::move(engine);
}

void FrameView::setFixedPick(std::shared_ptr<Pick> pick) {
    QMutexLocker lk(&sharedLock);
    fixedPick = std::move(pick);
}

FrameView::FrameView(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

FrameView::~FrameView() = default;

void FrameView::setCompare(int v) {
    if (v != 1 && v != 2) v = 0;
    if (v != compare_) { compare_ = v; emit pictureChanged(); update(); }
}
void FrameView::setCompareSplit(qreal v) {
    if (!std::isfinite(v)) return;
    v = std::clamp(v, qreal(0), qreal(1));
    if (v != compareSplit_) { compareSplit_ = v; emit pictureChanged(); update(); }
}
FrameView::Placement FrameView::place(double w, double h, double fw, double fh, int fill, const QRectF& keep,
                                      double zoom, const QPointF& shift) {
    Placement out;
    out.source = QRectF(0, 0, 1, 1);
    if (!(w > 0 && h > 0 && fw > 0 && fh > 0)) return out;
    const QRectF k = keep.isValid() ? keep.intersected(QRectF(0, 0, 1, 1)) : QRectF(0, 0, 1, 1);
    if (!(k.width() > 0 && k.height() > 0)) return out;
    const double z = std::isfinite(zoom) && zoom > 0 ? zoom : 1.0;
    // The kept part's size on screen: fitted (bars), filling the item (cut), or the item itself, then zoomed.
    const double kw = fw * k.width(), kh = fh * k.height();
    double sw = w, sh = h;
    if (fill != 2) {
        const double scale = fill == 1 ? std::max(w / kw, h / kh) : std::min(w / kw, h / kh);
        sw = kw * scale;
        sh = kh * scale;
    }
    sw *= z;
    sh *= z;
    const double cx = w / 2 + (std::isfinite(shift.x()) ? shift.x() : 0) * w;
    const double cy = h / 2 + (std::isfinite(shift.y()) ? shift.y() : 0) * h;
    const QRectF whole(cx - sw / 2, cy - sh / 2, sw, sh);
    // Cut to the item; the source follows the same cut.
    const QRectF shown = whole.intersected(QRectF(0, 0, w, h));
    if (shown.isEmpty()) { out.rect = QRectF(); return out; }
    out.rect = shown;
    out.source = QRectF(k.x() + (shown.x() - whole.x()) / sw * k.width(), k.y() + (shown.y() - whole.y()) / sh * k.height(),
                        shown.width() / sw * k.width(), shown.height() / sh * k.height());
    return out;
}

void FrameView::setKeep(const QRectF& v) {
    const QRectF k = v.isValid() && v.width() > 0 && v.height() > 0 ? v : QRectF(0, 0, 1, 1);
    if (k != keep_) { keep_ = k; emit pictureChanged(); update(); }
}
void FrameView::setZoom(qreal v) {
    v = std::isfinite(v) ? std::clamp(v, 0.05, 16.0) : 1.0;
    if (v != zoom_) { zoom_ = v; emit pictureChanged(); update(); }
}
void FrameView::setShift(const QPointF& v) {
    if (v != shift_) { shift_ = v; emit pictureChanged(); update(); }
}

void FrameView::setCompareSwap(bool v) { if (v != compareSwap_) { compareSwap_ = v; emit pictureChanged(); update(); } }
void FrameView::setBrightness(qreal v) { if (v != brightness_) { brightness_ = v; emit pictureChanged(); update(); } }
void FrameView::setContrast(qreal v) { if (v != contrast_) { contrast_ = v; emit pictureChanged(); update(); } }
void FrameView::setSaturation(qreal v) { if (v != saturation_) { saturation_ = v; emit pictureChanged(); update(); } }
void FrameView::setGamma(qreal v) { if (v != gamma_) { gamma_ = v; emit pictureChanged(); update(); } }
void FrameView::setFill(int v) {
    v = std::clamp(v, 0, 2);
    if (v == fill_) return;
    fill_ = v;
    emit pictureChanged();
    update();
}
void FrameView::setUpscale(bool v) { if (v != upscale_) { upscale_ = v; emit pictureChanged(); update(); } }
QSize FrameView::upscaleTargetSize(int index) {
    switch (index) {
    case 1: return {2560, 1440};
    case 2: return {3840, 2160};
    case 4: return {5120, 2880};
    case 5: return {6144, 3456};
    }
    return {};
}

void FrameView::setUpscaleTarget(int v) {
    if (upscaleTargetSize(v).isEmpty()) v = 2;
    if (v != upscaleTarget_) { upscaleTarget_ = v; emit pictureChanged(); update(); }
}
void FrameView::setSharpness(qreal v) {
    v = std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.5;
    if (v != sharpness_) { sharpness_ = v; emit pictureChanged(); update(); }
}

void FrameView::reportUpscale(int state, const QString& sizes) {
    if (state == reportedState_ && sizes == reportedSizes_) return;
    reportedState_ = state;
    reportedSizes_ = sizes;
    QMetaObject::invokeMethod(this, [this, state, sizes] {
        if (state == upscaleState_ && sizes == upscaleSizes_) return;
        upscaleState_ = state;
        upscaleSizes_ = sizes;
        emit upscaleStateChanged();
    }, Qt::QueuedConnection);
}

void FrameView::setRunning(bool v) {
    if (v == running_) return;
    running_ = v;
    emit runningChanged();
    update();
}

void FrameView::itemChange(ItemChange change, const ItemChangeData& value) {
    if (change == ItemSceneChange) {
        disconnect(swapped_);
        if (value.window) {
            // Redraw every refresh while running: frameSwapped comes from the render thread, the update
            // must be asked for on the GUI thread.
            swapped_ = connect(value.window, &QQuickWindow::frameSwapped, this, [this] { if (running_) update(); }, Qt::QueuedConnection);
            update();
        }
    }
    QQuickItem::itemChange(change, value);
}

void FrameView::releaseResources() {
    // Called on the GUI thread when the window goes away; textures must die on the render thread.
    // The scene graph deletes the node; the textures are dropped on the next sync (or leak with the
    // window, which takes its graphics device with it).
    QQuickItem::releaseResources();
}

void FrameView::dropTextures() {
    for (auto& c : cache_) delete c.texture;
    cache_.clear();
    delete black_;
    black_ = nullptr;
}

QSGTexture* FrameView::cached(quint64 key, const std::function<QImage()>& make, bool nearest) {
    for (auto& c : cache_)
        if (c.key == key) { c.lastUse = frameCounter_; return c.texture; }
    const QImage image = make();
    if (image.isNull()) return nullptr;
    QSGTexture* t = window()->createTextureFromImage(image);
    if (!t) return nullptr;
    t->setFiltering(nearest ? QSGTexture::Nearest : QSGTexture::Linear);
    t->setMipmapFiltering(QSGTexture::None);
    t->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    t->setVerticalWrapMode(QSGTexture::ClampToEdge);
    // Keep the newest few: two frames, their fields, mask and Crosshair Hold's three, and the next pair coming in.
    if (cache_.size() >= 18) {
        auto oldest = std::min_element(cache_.begin(), cache_.end(), [](const CachedTexture& a, const CachedTexture& b) { return a.lastUse < b.lastUse; });
        delete oldest->texture;
        cache_.erase(oldest);
    }
    cache_.push_back({key, t, frameCounter_});
    return t;
}

QSGNode* FrameView::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
    ++frameCounter_;
    QQuickWindow* win = window();
    if (!win) return old;
    std::shared_ptr<Engine> engine;
    std::shared_ptr<Pick> fixed;
    {
        QMutexLocker lk(&sharedLock);
        engine = sharedEngine;
        fixed = fixedPick;
    }
    Pick pick;
    if (fixed) {
        pick = *fixed;
    } else if (engine) {
        // The picture drawn now appears at the next refresh.
        const double hz = win->screen() && win->screen()->refreshRate() > 1 ? win->screen()->refreshRate() : 60.0;
        pick = engine->pick(nowNs() + qint64(1e9 / hz));
    }
    if (!pick.valid() || width() <= 0 || height() <= 0) {
        delete old;
        if (!pick.valid()) dropTextures();
        reportUpscale(upscale_ ? UpscaleWaiting : UpscaleOff, {});
        return nullptr;
    }
    // The picture in the item: whole with black bars, filling it with the sides cut, or stretched.
    // source is the part of the picture drawn, as shares of it.
    const double fw = pick.a->width, fh = pick.a->height;
    const Placement placed = place(width(), height(), fw, fh, fill_, keep_, zoom_, shift_);
    const QRectF rect = placed.rect, source = placed.source;
    if (rect.width() <= 0 || rect.height() <= 0) {  // moved wholly out of the window
        delete old;
        reportUpscale(upscale_ ? UpscaleNotNeeded : UpscaleOff, {});
        return nullptr;
    }
    const quint64 keyA = pick.aId << 4, keyB = pick.bId << 4;

    const bool software = win->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    if (software) {
        reportUpscale(upscale_ ? UpscaleUnsupported : UpscaleOff, {});
        // No shaders on the software renderer: the nearer real frame.
        auto* node = dynamic_cast<QSGImageNode*>(old);
        if (!node) { delete old; node = win->createImageNode(); }
        const bool useB = pick.generated() && pick.t >= 0.5f;
        QSGTexture* t = cached(useB ? keyB : keyA, [&] { return frameImage(useB ? pick.b : pick.a); }, false);
        if (!t) return node;
        node->setTexture(t);
        node->setOwnsTexture(false);
        node->setRect(rect);
        node->setSourceRect(QRectF(source.x() * fw, source.y() * fh, source.width() * fw, source.height() * fh));
        node->setFiltering(QSGTexture::Linear);
        node->markDirty(QSGNode::DirtyMaterial);
        return node;
    }

    if (!black_) {
        QImage b(1, 1, QImage::Format_RGBA8888);
        b.fill(Qt::black);
        black_ = win->createTextureFromImage(b);
    }
    QSGTexture* ta = cached(keyA, [&] { return frameImage(pick.a); }, false);
    QSGTexture* tb = pick.b != pick.a ? cached(keyB, [&] { return frameImage(pick.b); }, false) : ta;
    const bool generate = pick.generated() && ta && tb;
    QSGTexture *tf = black_, *tk = black_, *tm = black_;
    bool hud = false;
    if (generate) {
        const quint64 pairKey = ((pick.aId * 1000003ULL) ^ pick.bId) << 4;
        tf = cached(pairKey | 1, [&] { return fieldImage(pick.fields->forward); }, true);
        tk = cached(pairKey | 2, [&] { return fieldImage(pick.fields->backward); }, true);
        if (pick.fields->hudMask && !pick.fields->hudMask->empty()) {
            tm = cached(pairKey | 3, [&] { return maskImage(*pick.fields->hudMask); }, false);
            hud = tm != nullptr;
        }
        if (!tf || !tk) { tf = tk = black_; }
    }
    // Crosshair Hold's and Subtitle Hold's parts of this pair, when something is held (CrosshairHold.h).
    const auto whole = [](const std::shared_ptr<const HoldPair>& z) {
        return z && z->w > 1 && z->h > 1 && z->filledA.width == z->w && z->filledA.height == z->h &&
               z->filledB.width == z->w && z->filledB.height == z->h && z->shows.width == z->w && z->shows.height == z->h;
    };
    QSGTexture *tha = black_, *thb = black_, *ths = black_;
    bool holding = false;
    if (generate && tf != black_ && whole(pick.hold)) {
        const quint64 pairKey = ((pick.aId * 1000003ULL) ^ pick.bId) << 4;
        tha = cached(pairKey | 4, [&] { return holdImage(pick.hold, false); }, false);
        thb = cached(pairKey | 5, [&] { return holdImage(pick.hold, true); }, false);
        ths = cached(pairKey | 6, [&] { return maskImage(pick.hold->shows); }, false);
        holding = tha && thb && ths;
        if (!holding) tha = thb = ths = black_;
    }
    QSGTexture *tsa = black_, *tsb = black_, *tss = black_;
    bool subHolding = false;
    if (generate && tf != black_ && whole(pick.subHold)) {
        const quint64 pairKey = ((pick.aId * 1000003ULL) ^ pick.bId) << 4;
        tsa = cached(pairKey | 7, [&] { return holdImage(pick.subHold, false); }, false);
        tsb = cached(pairKey | 8, [&] { return holdImage(pick.subHold, true); }, false);
        tss = cached(pairKey | 9, [&] { return maskImage(pick.subHold->shows); }, false);
        subHolding = tsa && tsb && tss;
        if (!subHolding) tsa = tsb = tss = black_;
    }
    const bool interpolating = generate && tf != black_ && tk != black_;
    const std::array<QSGTexture*, 11> textures{ta ? ta : black_, tb ? tb : black_, tf, tk, tm ? tm : black_, tha, thb, ths, tsa, tsb, tss};
    Uniforms u{};
    u.phase = pick.t;
    u.mode = interpolating ? 1.f : 0.f;
    u.hudOn = interpolating && hud ? 1.f : 0.f;
    u.frameSize[0] = float(fw);
    u.frameSize[1] = float(fh);
    u.fieldGrid[0] = interpolating ? float(pick.fields->forward.blocksX) : 1.f;
    u.fieldGrid[1] = interpolating ? float(pick.fields->forward.blocksY) : 1.f;
    u.blockSize = interpolating ? float(pick.fields->forward.blockSize) : 16.f;
    u.errLow = 24.f;
    u.errHigh = 48.f;
    u.brightness = float(brightness_);
    u.contrast = float(contrast_);
    u.saturation = float(saturation_);
    u.gamma = float(gamma_);
    const bool held = interpolating && holding;
    u.holdZone[0] = held ? float(pick.hold->x) : 0.f;
    u.holdZone[1] = held ? float(pick.hold->y) : 0.f;
    u.holdZone[2] = held ? float(pick.hold->size) : 1.f;
    u.holdZone[3] = held ? 1.f : 0.f;
    const bool subHeld = interpolating && subHolding;
    u.subZone[0] = subHeld ? float(pick.subHold->x) : 0.f;
    u.subZone[1] = subHeld ? float(pick.subHold->y) : 0.f;
    u.subZone[2] = subHeld ? float(pick.subHold->w) : 1.f;
    u.subZone[3] = subHeld ? float(pick.subHold->h) : 1.f;
    u.subFlags[0] = subHeld ? 1.f : 0.f;

    const qreal dpr = win->effectiveDevicePixelRatio();
    // The live comparison. In window pixels (the line's width is the Windows build's, for the window's
    // height), then as shares of the picture, which is what both shaders compare their coordinate with.
    std::array<float, 4> compare{}, compareLine{};
    if (compare_ == 1) {
        compare = {1.f, -1.f, 2.f, 0.f};
    } else if (compare_ == 2 && rect.width() > 0) {
        const long left = std::lround(rect.left() * dpr), right = std::lround(rect.right() * dpr);
        const long split = std::lround(compareSplit_ * width() * dpr);
        const auto sides = veyra::engine::pushCompareSides(left, right, split, compareSwap_);
        const auto toU = [&](long px) { return float(source.x() + (px / dpr - rect.x()) / rect.width() * source.width()); };
        // The original's part reaches past the picture's own edge on its outer side.
        compare = {1.f, compareSwap_ ? toU(sides.referenceLeft) : -1.f, compareSwap_ ? 2.f : toU(sides.referenceRight), 0.f};
        const auto line = veyra::engine::pushCompareLine(sides.line, left, right, std::lround(win->height() * dpr));
        if (line.visible) {
            compare[3] = 1.f;
            compareLine = {toU(line.outerLeft), toU(line.outerRight), toU(line.coreLeft), toU(line.coreRight)};
        }
    }
    for (int i = 0; i < 4; ++i) { u.compare[i] = compare[size_t(i)]; u.compareLine[i] = compareLine[size_t(i)]; }

    // Upscaling: by how much the shown part of the picture is enlarged on screen, never more than NIS's
    // 2 per side nor past the size chosen (fitted to the picture's shape, as on Windows). What is left
    // above 2 the window scales as it always did.
    const double shown = std::min(rect.width() * dpr / (source.width() * fw), rect.height() * dpr / (source.height() * fh));
    const QSize target = upscaleTargetSize(upscaleTarget_);
    const double targetW = target.width(), targetH = target.height();
    const double scale = std::min({2.0, shown, std::min(targetW / fw, targetH / fh)});
    const QSize output(int(std::lround(fw * scale)), int(std::lround(fh * scale)));
    int state = UpscaleOff;
    if (upscale_) {
        if (auto* nis = dynamic_cast<NisNode*>(old); nis && nis->failed()) nisFailed_ = true;
        if (nisFailed_) state = UpscaleFailed;
        else if (!NisNode::supported(win)) state = UpscaleUnsupported;
        else if (scale < 1.05) state = UpscaleNotNeeded;  // a few per cent larger is not worth the work
        else state = UpscaleRunning;
    }
    reportUpscale(state, state == UpscaleRunning ? QStringLiteral("%1 × %2 → %3 × %4").arg(int(fw)).arg(int(fh)).arg(output.width()).arg(output.height())
                                                 : QString());

    if (state == UpscaleRunning) {
        auto* nis = dynamic_cast<NisNode*>(old);
        if (!nis) { delete old; nis = new NisNode(win); }
        NisFrame frame;
        frame.textures = textures;
        frame.nearest = {false, false, true, true, false, false, false, false, false, false, false};  // the two motion fields
        // The line is drawn after upscaling (blit.frag), never into the picture NIS sharpens.
        Uniforms beforeNis = u;
        beforeNis.compare[3] = 0.f;
        frame.uniforms = QByteArray(reinterpret_cast<const char*>(&beforeNis), sizeof(Uniforms));
        frame.compare = compare;
        frame.compareLine = compareLine;
        frame.picture = QSize(int(fw), int(fh));
        frame.output = output;
        frame.rect = rect;
        frame.source = source;
        frame.sharpness = float(sharpness_);
        nis->set(frame);
        return nis;
    }

    auto* node = dynamic_cast<FrameNode*>(old);
    if (!node) { delete old; node = new FrameNode; }
    FrameMaterial& m = node->material_;
    for (size_t i = 0; i < textures.size(); ++i) m.textures[i] = textures[i];
    m.u = u;
    QSGGeometry::updateTexturedRectGeometry(&node->geometry_, rect, source);
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}

}  // namespace pushfps

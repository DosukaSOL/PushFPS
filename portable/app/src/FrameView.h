// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// The picture. Once per display refresh it asks the engine what to show at that moment and draws it
// with the interpolation shader (shaders/frame.frag) through Qt's renderer (Vulkan or OpenGL on Linux,
// Metal on macOS). On Qt's software renderer it shows the real frames only.
#include "veyra/portable/Engine.h"
#include "veyra/portable/Protection.h"

#include <QQuickItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>
#include <vector>

class QSGTexture;

namespace pushfps {

class FrameView : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY pictureChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY pictureChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY pictureChanged)
    Q_PROPERTY(qreal gamma READ gamma WRITE setGamma NOTIFY pictureChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    // How a picture of another shape than the window is shown: 0 whole with bars, 1 the window filled
    // and the picture's sides cut, 2 stretched.
    Q_PROPERTY(int fill READ fill WRITE setFill NOTIFY pictureChanged)
    // Crop and resize (the Windows build's PreviewView crop, preferences in PushCrop.js): `keep` is the part
    // of the picture shown, as shares of it; that part is fitted, filled or stretched as `fill` says, made
    // `zoom` times as large and moved by `shift` (shares of this item's width and height). With the whole
    // picture, 1 and (0, 0) the picture is drawn exactly as without them.
    Q_PROPERTY(QRectF keep READ keep WRITE setKeep NOTIFY pictureChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY pictureChanged)
    Q_PROPERTY(QPointF shift READ shift WRITE setShift NOTIFY pictureChanged)
    // HUD protection boxes (portable/core Protection.h, the Windows build's "Exclude all effects" regions): a list
    // of {left, top, right, bottom, ellipse} as shares of the picture, at most four; generated frames show the
    // older real frame inside them. `protectFeather` softens their edges (picture pixels).
    Q_PROPERTY(QVariantList protect READ protect WRITE setProtect NOTIFY pictureChanged)
    Q_PROPERTY(qreal protectFeather READ protectFeather WRITE setProtectFeather NOTIFY pictureChanged)
    // The live comparison with the original (the picture before the picture controls and NIS), as the
    // Windows build's compareMode: 0 off, 2 split, 1 the original alone. The split stands at compareSplit
    // (a share of this item's width; the line itself is engine/PushCompareLine.h's); compareSwap puts
    // the original on the right.
    Q_PROPERTY(int compare READ compare WRITE setCompare NOTIFY pictureChanged)
    Q_PROPERTY(qreal compareSplit READ compareSplit WRITE setCompareSplit NOTIFY pictureChanged)
    Q_PROPERTY(bool compareSwap READ compareSwap WRITE setCompareSwap NOTIFY pictureChanged)
    // Upscaling with NVIDIA Image Scaling (NisNode.h): on or off, the size it upscales towards (the
    // Windows build's srTargetIndex: upscaleTargetSize) and its sharpening (0..1).
    Q_PROPERTY(bool upscale READ upscale WRITE setUpscale NOTIFY pictureChanged)
    Q_PROPERTY(int upscaleTarget READ upscaleTarget WRITE setUpscaleTarget NOTIFY pictureChanged)
    Q_PROPERTY(qreal sharpness READ sharpness WRITE setSharpness NOTIFY pictureChanged)
    // What upscaling does now (UpscaleState) and, while it runs, the sizes ("1280 × 720 → 2560 × 1440").
    Q_PROPERTY(int upscaleState READ upscaleState NOTIFY upscaleStateChanged)
    Q_PROPERTY(QString upscaleSizes READ upscaleSizes NOTIFY upscaleStateChanged)
public:
    enum UpscaleState {
        UpscaleOff = 0,          // switched off
        UpscaleRunning = 1,      // the picture is upscaled
        UpscaleNotNeeded = 2,    // the picture is not shown larger than it is (or already at the target)
        UpscaleUnsupported = 3,  // this renderer cannot run a compute shader (OpenGL before 4.3, software)
        UpscaleFailed = 4,       // something could not be made on this graphics card
        UpscaleWaiting = 5,      // nothing is playing
    };
    Q_ENUM(UpscaleState)

    explicit FrameView(QQuickItem* parent = nullptr);
    ~FrameView() override;

    qreal brightness() const { return brightness_; }
    qreal contrast() const { return contrast_; }
    qreal saturation() const { return saturation_; }
    qreal gamma() const { return gamma_; }
    bool running() const { return running_; }
    int fill() const { return fill_; }
    void setFill(int v);
    QRectF keep() const { return keep_; }
    qreal zoom() const { return zoom_; }
    QPointF shift() const { return shift_; }
    void setKeep(const QRectF& v);
    void setZoom(qreal v);
    void setShift(const QPointF& v);
    QVariantList protect() const { return protect_; }
    qreal protectFeather() const { return protection_.feather; }
    void setProtect(const QVariantList& v);
    void setProtectFeather(qreal v);
    // Where the kept part of a picture fw x fh large stands in an item w x h large (`rect`, which can reach
    // past the item) and the part of the picture drawn there (`source`, shares) once it is cut to the item.
    struct Placement { QRectF rect, source; };
    static Placement place(double w, double h, double fw, double fh, int fill, const QRectF& keep, double zoom, const QPointF& shift);
    int compare() const { return compare_; }
    qreal compareSplit() const { return compareSplit_; }
    bool compareSwap() const { return compareSwap_; }
    void setCompare(int v);
    void setCompareSplit(qreal v);
    void setCompareSwap(bool v);
    void setBrightness(qreal v);
    void setContrast(qreal v);
    void setSaturation(qreal v);
    void setGamma(qreal v);
    void setRunning(bool v);
    bool upscale() const { return upscale_; }
    int upscaleTarget() const { return upscaleTarget_; }
    qreal sharpness() const { return sharpness_; }
    int upscaleState() const { return upscaleState_; }
    QString upscaleSizes() const { return upscaleSizes_; }
    void setUpscale(bool v);
    void setUpscaleTarget(int v);
    void setSharpness(qreal v);
    // The sizes upscaling can aim at, by the Windows build's srTargetIndex and with its sizes
    // (ResolutionPlan.h srTargetExtent): 1 = 2K 2560 x 1440, 2 = 4K 3840 x 2160, 4 = 5K 5120 x 2880,
    // 5 = 6K 6144 x 3456. Any other index (3 = 8K, 6 = 7K on Windows) gives an empty size.
    static QSize upscaleTargetSize(int index);

    // The engine every FrameView draws from (set once by the application).
    static void setEngine(std::shared_ptr<veyra::portable::Engine> engine);
    // GPU check: draw this fixed pick instead of asking the engine.
    static void setFixedPick(std::shared_ptr<veyra::portable::Pick> pick);

signals:
    void pictureChanged();
    void runningChanged();
    void upscaleStateChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void releaseResources() override;

private:
    struct CachedTexture {
        quint64 key = 0;
        QSGTexture* texture = nullptr;
        quint64 lastUse = 0;
    };
    QSGTexture* cached(quint64 key, const std::function<QImage()>& make, bool nearest);
    void dropTextures();
    // From the render thread: what upscaling does, handed to the GUI thread.
    void reportUpscale(int state, const QString& sizes);

    qreal brightness_ = 0, contrast_ = 1, saturation_ = 1, gamma_ = 1;
    bool running_ = true;
    int fill_ = 0;
    QRectF keep_{0, 0, 1, 1};
    qreal zoom_ = 1;
    QPointF shift_;
    QVariantList protect_;
    veyra::portable::Protection protection_;
    int compare_ = 0;
    qreal compareSplit_ = 0.5;
    bool compareSwap_ = false;
    bool upscale_ = false;
    int upscaleTarget_ = 2;
    qreal sharpness_ = 0.5;
    int upscaleState_ = UpscaleOff;
    QString upscaleSizes_;
    // Render thread: the last state handed over, and whether NIS failed on this window.
    int reportedState_ = -1;
    QString reportedSizes_;
    bool nisFailed_ = false;
    std::vector<CachedTexture> cache_;
    quint64 frameCounter_ = 0;
    QSGTexture* black_ = nullptr;
    QMetaObject::Connection swapped_;
};

}  // namespace pushfps

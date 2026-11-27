// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// PushFPS: the keyboard and mouse of this PC as a controller for the console a session plays
// (remoteplay/PushKbm.h has the translation). Pattern of MoonlightInputCapture: while captured, the app
// window's keyboard and mouse messages are read by the native event filter before Qt sees them, the mouse
// as raw relative movement, the cursor hidden and kept inside the window.
//
// Captured: every key a control has, every mouse button and the wheel go to the console and nowhere else;
// a function key no control has still reaches PushFPS (its shortcuts: screenshot, record, replay ...); the
// release key (F12 unless chosen) ends the capture. Losing the focus (Alt+Tab, the Windows key, another
// window) ends it too and lets go of everything. Not captured, the release key starts it while a session
// can take it (`setAvailable`), and so does `capture(true)` (a click on the picture).
// Listening (the settings ask for a key): the next key pressed in the PushFPS window is reported and
// reaches nothing else; mouse inputs are chosen from a list, so a click can still cancel.
// While captured the hidden cursor is held at the middle of the picture (`setPicture`), never anywhere
// else: the window has controls of its own floating over the picture (the full screen bar is a window
// of its own), and a cursor left free to wander under raw mouse movement would click one of them with
// the left button and end the capture. Raw input reports the mouse's own movement, so holding the cursor
// changes nothing of what the console gets.
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <windows.h>

#include <cstdint>

#include "veyra/remoteplay/PushKbm.h"

namespace veyra::ui {

class PushKbmCapture final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit PushKbmCapture(QObject* parent = nullptr);
    ~PushKbmCapture() override;

    void setWindow(HWND topLevel) { window_ = topLevel; }
    // The window the picture is drawn in (a child of the top-level window): its middle is where the
    // hidden cursor is held while captured. None: the middle of the top-level window.
    void setPicture(HWND picture) { picture_ = picture; }
    void setSettings(const remoteplay::kbm::Settings& settings);
    const remoteplay::kbm::Settings& settings() const { return mixer_.settings(); }
    // A session can take the keyboard and mouse now; false also ends a capture.
    void setAvailable(bool available);
    bool available() const { return available_; }
    bool captured() const { return captured_; }
    // False when it cannot start: not available, no window, or the window not in front.
    bool capture(bool on);
    // The state to send: the controller's, with the keyboard and mouse added while captured.
    remoteplay::ControllerState apply(const remoteplay::ControllerState& pad, remoteplay::kbm::Console console);

    // The next key pressed answers for this control's input slot; control kControls asks for the release key.
    void listen(int control, int slot);
    void stopListening();
    int listeningControl() const { return listenControl_; }
    int listeningSlot() const { return listenSlot_; }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    // The code of a key message: Shift, Ctrl and Alt as their left or right keys.
    static std::uint16_t keyCode(WPARAM wParam, LPARAM lParam);
    // A name for an input code, from the keyboard layout where Windows has one.
    static QString inputName(std::uint16_t code);

signals:
    void capturedChanged(bool captured);
    void input();                                           // a key, button or the wheel changed: send it now
    void moved();                                           // the mouse moved (up to thousands a second)
    void releaseRequested();                                // the release key ended the capture
    void notReady();                                        // the release key while switched on and no session can take them
    void bindingHeard(int control, int slot, int code);     // the key pressed while listening
    void listeningChanged();

private:
    void clipToWindow();
    bool keyMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static std::int64_t nowUs();

    remoteplay::kbm::Mixer mixer_;
    HWND window_ = nullptr;
    HWND picture_ = nullptr;
    bool available_ = false;
    bool captured_ = false;
    std::uint16_t swallowUp_ = 0;   // the key-up of the key that started or ended the capture
    int wheelRest_ = 0;             // a fine-scrolling wheel's steps short of a notch
    int listenControl_ = -1, listenSlot_ = -1;
};

} // namespace veyra::ui

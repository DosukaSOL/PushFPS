// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/ui/PushKbmCapture.h"

#include <QCoreApplication>
#include <QGuiApplication>

#include <chrono>
#include <format>

#include "veyra/Log.h"

namespace veyra::ui {

namespace kbm = remoteplay::kbm;

PushKbmCapture::PushKbmCapture(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
    // Focus lost (Alt+Tab, the Windows key, another window on top): give the keyboard and mouse back at once.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            if (captured_) capture(false);
            if (listenControl_ >= 0) stopListening();
        }
    });
}

PushKbmCapture::~PushKbmCapture() {
    capture(false);
    if (auto* app = QCoreApplication::instance()) app->removeNativeEventFilter(this);
}

std::int64_t PushKbmCapture::nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void PushKbmCapture::setSettings(const kbm::Settings& settings) {
    const bool wasOn = mixer_.settings().on;
    mixer_.configure(settings);
    // A changed map must not leave something held that no key lets go of any more.
    mixer_.clear();
    if (captured_) emit input();
    if (wasOn && !mixer_.settings().on && captured_) capture(false);
}

void PushKbmCapture::setAvailable(bool available) {
    if (available_ == available) return;
    available_ = available;
    if (!available_ && captured_) capture(false);
}

bool PushKbmCapture::capture(bool on) {
    if (on == captured_) return true;
    if (on) {
        if (!available_ || !mixer_.settings().on || !window_ || GetForegroundWindow() != window_) return false;
        RAWINPUTDEVICE device{0x01, 0x02, 0, window_};   // generic desktop / mouse
        if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
            veyra::log::warn("kbm", "RegisterRawInputDevices failed; mouse and keyboard not taken");
            return false;
        }
        mixer_.clear();
        wheelRest_ = 0;
        captured_ = true;
        clipToWindow();
        SetCursor(nullptr);
        veyra::log::info("kbm", "captured");
    } else {
        captured_ = false;
        RAWINPUTDEVICE device{0x01, 0x02, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&device, 1, sizeof(device));
        ClipCursor(nullptr);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        mixer_.clear();
        veyra::log::info("kbm", "released");
    }
    emit capturedChanged(captured_);
    emit input();   // released: nothing stays pressed on the console
    return true;
}

remoteplay::ControllerState PushKbmCapture::apply(const remoteplay::ControllerState& pad, kbm::Console console) {
    if (!captured_) return pad;
    return mixer_.apply(pad, nowUs(), console);
}

void PushKbmCapture::listen(int control, int slot) {
    // control kControls is the release key itself (slot 0).
    if (control < 0 || control > int(kbm::kControls) || slot < 0 || slot >= int(kbm::kSlots)) return;
    if (control == int(kbm::kControls) && slot != 0) return;
    if (captured_) capture(false);
    listenControl_ = control;
    listenSlot_ = slot;
    emit listeningChanged();
}

void PushKbmCapture::stopListening() {
    if (listenControl_ < 0) return;
    listenControl_ = listenSlot_ = -1;
    emit listeningChanged();
}

void PushKbmCapture::clipToWindow() {
    RECT client{};
    if (!window_ || !GetClientRect(window_, &client)) return;
    POINT topLeft{client.left, client.top}, bottomRight{client.right, client.bottom};
    ClientToScreen(window_, &topLeft);
    ClientToScreen(window_, &bottomRight);
    const RECT screen{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&screen);
}

std::uint16_t PushKbmCapture::keyCode(WPARAM wParam, LPARAM lParam) {
    const auto vkey = std::uint16_t(wParam & 0xFFFF);
    const UINT scan = UINT((lParam >> 16) & 0xFF);
    const bool extended = ((lParam >> 24) & 1) != 0;
    switch (vkey) {
    case VK_SHIFT: return scan == 0x36 ? VK_RSHIFT : VK_LSHIFT;
    case VK_CONTROL: return extended ? VK_RCONTROL : VK_LCONTROL;
    case VK_MENU: return extended ? VK_RMENU : VK_LMENU;
    default: return kbm::isKey(vkey) ? vkey : 0;
    }
}

QString PushKbmCapture::inputName(std::uint16_t code) {
    switch (code) {
    case kbm::kNone: return QString();
    case kbm::kMouseLeft: return QObject::tr("鼠标左键");
    case kbm::kMouseRight: return QObject::tr("鼠标右键");
    case kbm::kMouseMiddle: return QObject::tr("鼠标中键");
    case kbm::kMouseX1: return QObject::tr("鼠标侧键（后）");
    case kbm::kMouseX2: return QObject::tr("鼠标侧键（前）");
    case kbm::kWheelUp: return QObject::tr("滚轮向上");
    case kbm::kWheelDown: return QObject::tr("滚轮向下");
    default: break;
    }
    if (!kbm::isKey(code)) return QString();
    // The keyboard layout's own name. Keys whose scan code is the same as another's (the arrows and the
    // number pad, the right Ctrl and Alt) need the extended bit to be named apart.
    UINT scan = MapVirtualKeyW(code, MAPVK_VK_TO_VSC_EX);
    if (scan == 0) return QStringLiteral("VK %1").arg(uint(code), 2, 16, QLatin1Char('0')).toUpper();
    bool extended = (scan & 0xE000) == 0xE000 || (scan & 0xE100) == 0xE100;
    switch (code) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK: case VK_RCONTROL: case VK_RMENU:
        extended = true; break;
    default: break;
    }
    LONG lparam = LONG((scan & 0xFF) << 16);
    if (extended) lparam |= LONG(1) << 24;
    wchar_t name[64]{};
    const int n = GetKeyNameTextW(lparam, name, 64);
    if (n <= 0) return QStringLiteral("VK %1").arg(uint(code), 2, 16, QLatin1Char('0')).toUpper();
    QString text = QString::fromWCharArray(name, n);
    // Windows names the right Shift, Ctrl and Alt as such and the left ones plainly; say which.
    if (code == VK_LSHIFT || code == VK_LCONTROL || code == VK_LMENU) text = QObject::tr("左 %1").arg(text);
    return text;
}

bool PushKbmCapture::keyMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    const bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    const bool repeat = down && ((lParam >> 30) & 1) != 0;
    const std::uint16_t code = keyCode(wParam, lParam);
    if (code == 0) return captured_;
    const auto& s = mixer_.settings();
    if (code == s.release) {
        if (down && !repeat) {
            if (captured_) { capture(false); emit releaseRequested(); }
            else capture(true);
            swallowUp_ = code;
        } else if (!down && swallowUp_ == code) {
            swallowUp_ = 0;
        }
        return true;
    }
    if (!down && swallowUp_ == code) { swallowUp_ = 0; return true; }
    if (!captured_) return false;
    // A function key no control has stays PushFPS's (screenshot, record, replay, the readout).
    if (kbm::isFunctionKey(code) && !mixer_.bound(code)) return false;
    if (!repeat) {
        mixer_.press(code, down);
        if (mixer_.bound(code)) emit input();
    }
    return true;
}

bool PushKbmCapture::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType != "windows_generic_MSG") return false;
    const MSG* msg = static_cast<const MSG*>(message);
    if (!window_ || !msg->hwnd || GetAncestor(msg->hwnd, GA_ROOT) != window_) return false;
    const UINT m = msg->message;
    const bool isKey = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP;

    // Listening for a binding: the next key pressed is the answer and reaches nothing else.
    if (listenControl_ >= 0) {
        if (isKey) {
            if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
                const std::uint16_t code = keyCode(msg->wParam, msg->lParam);
                if (code != 0) {
                    const int control = listenControl_, slot = listenSlot_;
                    listenControl_ = listenSlot_ = -1;
                    swallowUp_ = code;
                    emit listeningChanged();
                    emit bindingHeard(control, slot, code);
                }
            }
            if (result) *result = 0;
            return true;
        }
        if (m == WM_CHAR || m == WM_SYSCHAR || m == WM_DEADCHAR || m == WM_SYSDEADCHAR) { if (result) *result = 0; return true; }
        if (m == WM_SYSCOMMAND && (msg->wParam & 0xFFF0) == SC_KEYMENU) { if (result) *result = 0; return true; }
        return false;
    }

    if (!captured_) {
        // Not captured: only the release key (to start) and the key-up of what ended a capture.
        if (!isKey || !available_ || !mixer_.settings().on) {
            if (isKey && swallowUp_ && (m == WM_KEYUP || m == WM_SYSKEYUP) && keyCode(msg->wParam, msg->lParam) == swallowUp_) {
                swallowUp_ = 0;
                if (result) *result = 0;
                return true;
            }
            return false;
        }
        const bool consumed = keyMessage(m, msg->wParam, msg->lParam);
        if (consumed && result) *result = 0;
        return consumed;
    }

    switch (m) {
    case WM_INPUT: {
        alignas(8) BYTE buffer[128];
        UINT size = sizeof(buffer);
        const UINT got = GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER));
        if (got != UINT(-1)) {
            const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer);
            if (raw->header.dwType == RIM_TYPEMOUSE && (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 &&
                (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0)) {
                mixer_.move(int(raw->data.mouse.lLastX), int(raw->data.mouse.lLastY), nowUs());
                emit moved();
            }
        }
        if (result) *result = 0;
        return true;
    }
    case WM_SETCURSOR:
        SetCursor(nullptr);
        if (result) *result = TRUE;
        return true;
    case WM_SIZE: case WM_MOVE:
        clipToWindow();
        return false;   // the interface still needs the resize
    case WM_ACTIVATE:
        if (LOWORD(msg->wParam) == WA_INACTIVE) capture(false);
        return false;
    case WM_KILLFOCUS:
        capture(false);
        return false;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
        const bool consumed = keyMessage(m, msg->wParam, msg->lParam);
        if (consumed && result) *result = 0;
        return consumed;
    }
    case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR: case WM_UNICHAR: case WM_IME_CHAR:
        if (result) *result = 0;
        return true;   // a key's character never reaches the interface underneath
    case WM_SYSCOMMAND:
        if ((msg->wParam & 0xFFF0) == SC_KEYMENU) { if (result) *result = 0; return true; }   // Alt must not open the window menu
        return false;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: case WM_XBUTTONUP: {
        std::uint16_t code = kbm::kNone;
        bool down = true;
        switch (m) {
        case WM_LBUTTONUP: down = false; [[fallthrough]];
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: code = kbm::kMouseLeft; break;
        case WM_RBUTTONUP: down = false; [[fallthrough]];
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: code = kbm::kMouseRight; break;
        case WM_MBUTTONUP: down = false; [[fallthrough]];
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: code = kbm::kMouseMiddle; break;
        case WM_XBUTTONUP: down = false; [[fallthrough]];
        default: code = GET_XBUTTON_WPARAM(msg->wParam) == XBUTTON1 ? kbm::kMouseX1 : kbm::kMouseX2; break;
        }
        mixer_.press(code, down);
        if (mixer_.bound(code)) emit input();
        if (result) *result = (m == WM_XBUTTONDOWN || m == WM_XBUTTONUP || m == WM_XBUTTONDBLCLK) ? TRUE : 0;
        return true;
    }
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
        if (delta != 0) {
            // One press a notch; a fine-scrolling wheel's small steps add up to notches of 120.
            wheelRest_ += delta;
            const int notches = wheelRest_ / WHEEL_DELTA;
            wheelRest_ -= notches * WHEEL_DELTA;
            if (notches != 0) { mixer_.wheel(notches); emit input(); }
        }
        if (result) *result = 0;
        return true;
    }
    case WM_MOUSEHWHEEL: case WM_MOUSEMOVE: case WM_NCMOUSEMOVE:
        if (result) *result = 0;
        return true;   // movement comes from raw input
    default:
        return false;
    }
}

} // namespace veyra::ui

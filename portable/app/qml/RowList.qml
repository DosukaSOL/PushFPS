// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// A scrolling list of rows for the preview's dialogs. A row's hover light (VRow.qml) reaches `room` pixels past
// the row on each side, and a list clips what it holds: rows laid out to the list's own edges had their light
// cut on the left (the row's left margin looked off) and run under the scroll bar on the right. This list keeps
// that room on both sides and the scroll bar's width beside it, so a dialog built on it cannot get that wrong.
// The dialog gives up the same room from its own padding (leftPadding / rightPadding: 20 - room), so its text
// stands where it stood. `PushFPS --layoutcheck` checks every page and dialog for a cut light.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: list
    // How far a row's light reaches past the row (VRow.qml), and the gap left before the scroll bar.
    readonly property int room: 7
    readonly property int gap: 4
    default property alias content: column.data
    property alias spacing: column.spacing
    implicitHeight: column.implicitHeight
    contentHeight: column.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: VScrollBar { id: bar }
    ColumnLayout {
        id: column
        x: list.room
        width: Math.max(0, list.width - 2 * list.room - bar.width - list.gap)
        spacing: 0
    }
}

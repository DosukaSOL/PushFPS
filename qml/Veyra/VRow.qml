import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: row
    property string label: ""
    property string hint: ""
    property color hintColor: Theme.t3
    property string value: ""
    // Narrow panels with a wide control (导出 输出设置): the hint runs under the
    // whole row instead of being squeezed beside the control.
    property bool hintBelow: false
    // PushFPS: a hint also moves under the row when the control takes a large part
    // of it (a segmented control or a select in the 376-440px side panel). Beside
    // such a control the hint was squeezed into a 120px column of ten lines.
    readonly property bool hintUnder: hint.length > 0 && (hintBelow
        || (!stacked && width > 0 && slot.childrenRect.width > width * 0.4))
    readonly property bool stacked: width > 0 && width - slot.childrenRect.width - (value.length ? 52 : 0) - 12
        < Math.min(170, labelMetrics.advanceWidth(label))
    FontMetrics { id: labelMetrics; font.family: Theme.fontUi; font.pixelSize: Theme.fsBody }
    default property alias control: slot.data
    // The row's slider, when its control is one (typed values beside the number).
    readonly property Item valueSlider: {
        for (let i = 0; i < slot.children.length; ++i) {
            const c = slot.children[i]
            if (c && c.commitValue !== undefined && c.inputDecimals !== undefined) return c
        }
        return null
    }

    // The layout reads Layout.preferredHeight. `height` is deliberately NOT set: a
    // layout-managed item that sets its own height is undefined behaviour, and this
    // row is only ever placed inside a layout.
    // A hint that wraps (narrow panels) grows the row instead of running into
    // the next one.
    // Vertical padding: the main line is centred in the 40px minimum; taller content
    // (wrapped labels, sliders) keeps 7px above and below.
    readonly property real topPad: Math.max(7, (Theme.rowMinHeight - mainRow.implicitHeight) / 2)
    implicitHeight: topPad * 2 + mainRow.implicitHeight
        + (hintUnder ? belowHint.implicitHeight + 2 : 0)
    Layout.preferredHeight: implicitHeight
    Layout.fillWidth: true
    GridLayout {
        id: mainRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: row.topPad
        columns: row.stacked ? 2 : 3
        columnSpacing: 12
        rowSpacing: 6
        ColumnLayout {
            id: textColumn
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.columnSpan: row.stacked ? 2 : 1
            spacing: 2
            // fillWidth here too: without a hint the label is the column's only child,
            // and a layout with no filling child cannot grow, which left the control
            // beside the label instead of at the row's right edge.
            // Grow the row for long labels instead of hiding translated text.
            Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: row.label; color: Theme.tLabel; font.family: Theme.fontUi; font.pixelSize: Theme.fsBody; wrapMode: Text.Wrap }
            Text {
                visible: row.hint.length > 0 && !row.hintUnder
                text: row.hint
                color: row.hintColor
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsHint
                lineHeight: 1.1
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        Item {
            id: slot
            implicitHeight: childrenRect.height
            Layout.preferredWidth: childrenRect.width
            Layout.preferredHeight: childrenRect.height
        }
        // A row whose control is a slider: its number can be clicked and typed.
        VSliderValue {
            visible: row.value.length > 0
            text: row.value
            slider: row.valueSlider
            Layout.minimumWidth: 40
        }
    }
    Text {
        id: belowHint
        visible: row.hintUnder
        anchors.left: parent.left
        anchors.right: parent.right
        // Under the control, the hint is still the label's: it stops short of the
        // right edge so it does not read as a caption of the control.
        anchors.rightMargin: 24
        anchors.top: mainRow.bottom
        anchors.topMargin: 2
        text: row.hint
        color: row.hintColor
        font.family: Theme.fontUi
        font.pixelSize: Theme.fsHint
        lineHeight: 1.1
        wrapMode: Text.WordWrap
    }
}

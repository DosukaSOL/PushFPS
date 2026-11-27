// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS 4.5.8: the Discord status's rows (Settings → Recording → Instant replay and sharing). part "status"
// is the switch (preference pushDiscordPresence, off unless switched on), and its hint says what the
// connection is doing (veyra.pushDiscord: off, looking, connected, refused; showing; picture). part "game"
// is shown while it is on: whether the chosen game profile's name is in the line (pushDiscordGame, on unless
// switched off). What is shown and how it is sent is ui/PushDiscordPresence.h.
import QtQuick

VRow {
    id: discordRow
    property string part: "status"
    readonly property bool on: veyra.preferences.pushDiscordPresence === true
    readonly property var st: veyra.pushDiscord || ({})
    objectName: part === "game" ? "set-discord-presence-game-row" : "set-discord-presence-row"
    visible: part !== "game" || on
    label: part === "game" ? qsTr("显示游戏名称") : qsTr("Discord 状态")
    hintBelow: part !== "game"
    hint: {
        if (part === "game") return qsTr("显示所选游戏配置的名称；关闭后只显示主机和帧率")
        if (!on || st.on !== true)
            return qsTr("在 Discord 个人资料上向好友显示你正在用 PushFPS 玩什么：游戏配置的名称、主机和帧率。只在播放主机画面时显示，需要这台电脑上运行 Discord")
        if (st.state === "refused") return qsTr("Discord 不认识 PushFPS 的应用 ID，请更新 PushFPS")
        if (st.state !== "connected") return qsTr("没有找到 Discord：请在这台电脑上打开 Discord 应用（网页版不行）")
        if (st.showing === true) return qsTr("正在 Discord 上显示。好友看不到时，请在 Discord 的设置 → 活动隐私中打开活动分享")
        return st.picture === true ? qsTr("已连接 Discord：马上显示") : qsTr("已连接 Discord：播放主机画面时显示")
    }
    hintColor: part !== "game" && on && st.on === true && st.state !== "connected" ? Theme.warn : Theme.t3
    VSwitch {
        objectName: discordRow.part === "game" ? "set-discord-presence-game" : "set-discord-presence"
        checked: discordRow.part === "game" ? veyra.preferences.pushDiscordGame !== false : discordRow.on
        onToggled: v => veyra.setPreference(discordRow.part === "game" ? "pushDiscordGame" : "pushDiscordPresence", v)
    }
}

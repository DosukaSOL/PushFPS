// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The Discord status's rows (PushDiscordRow) against the fixture bridge: off unless switched on, the game
// profile's name on unless switched off, and a hint that says what the connection is doing.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 760; height: 400
    VGroup {
        width: 700
        PushDiscordRow { id: status; part: "status" }
        PushDiscordRow { id: game; part: "game" }
    }

    TestCase {
        name: "PushDiscord"
        when: windowShown
        function init() {
            veyra.preferences = ({})
            veyra.pushDiscord = ({on: false, state: "off", showing: false, picture: false})
        }
        function control(row, name) { const c = findChild(row, name); verify(c !== null, name); return c }

        function test_off_until_switched_on_and_the_game_name_follows() {
            compare(status.objectName, "set-discord-presence-row")
            verify(control(status, "set-discord-presence").checked === false, "nothing saved: off")
            compare(status.hint, qsTr("在 Discord 个人资料上向好友显示你正在用 PushFPS 玩什么：游戏配置的名称、主机和帧率。只在播放主机画面时显示，需要这台电脑上运行 Discord"))
            verify(!game.visible, "the game row only while the status is on")
            control(status, "set-discord-presence").toggled(true)
            verify(veyra.preferences.pushDiscordPresence === true)
            tryVerify(function() { return game.visible }, 3000)
            compare(game.objectName, "set-discord-presence-game-row")
            verify(control(game, "set-discord-presence-game").checked === true, "the game's name is shown unless switched off")
            control(game, "set-discord-presence-game").toggled(false)
            verify(veyra.preferences.pushDiscordGame === false)
            verify(control(game, "set-discord-presence-game").checked === false)
            control(status, "set-discord-presence").toggled(false)
            verify(veyra.preferences.pushDiscordPresence === false)
            tryVerify(function() { return !game.visible }, 3000)
        }
        function test_the_hint_says_what_the_connection_does() {
            veyra.setPreference("pushDiscordPresence", true)
            // Switched on a moment ago: the bridge has not answered yet; nothing is said about Discord.
            compare(status.hint, qsTr("在 Discord 个人资料上向好友显示你正在用 PushFPS 玩什么：游戏配置的名称、主机和帧率。只在播放主机画面时显示，需要这台电脑上运行 Discord"))
            veyra.pushDiscord = ({on: true, state: "looking", showing: false, picture: false})
            compare(status.hint, qsTr("没有找到 Discord：请在这台电脑上打开 Discord 应用（网页版不行）"))
            compare(status.hintColor, Theme.warn, "said as a warning")
            veyra.pushDiscord = ({on: true, state: "connected", showing: false, picture: false})
            compare(status.hint, qsTr("已连接 Discord：播放主机画面时显示"))
            compare(status.hintColor, Theme.t3)
            veyra.pushDiscord = ({on: true, state: "connected", showing: false, picture: true})
            compare(status.hint, qsTr("已连接 Discord：马上显示"))
            veyra.pushDiscord = ({on: true, state: "connected", showing: true, picture: true})
            compare(status.hint, qsTr("正在 Discord 上显示。好友看不到时，请在 Discord 的设置 → 活动隐私中打开活动分享"))
            veyra.pushDiscord = ({on: true, state: "refused", showing: false, picture: true})
            compare(status.hint, qsTr("Discord 不认识 PushFPS 的应用 ID，请更新 PushFPS"))
            compare(status.hintColor, Theme.warn)
            compare(game.hint, qsTr("显示所选游戏配置的名称；关闭后只显示主机和帧率"))
        }
    }
}

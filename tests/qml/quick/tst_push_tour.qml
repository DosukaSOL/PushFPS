// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The first-run tour (PushTour.qml): its steps and their order, when it offers itself, and what
// the quick setup hands it to point at. The tour's own machinery (OnboardingGuide.qml) is walked
// through with PushFPS's steps here; its spotlight, keys and modality have their own test.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    // High enough for the quick setup's short form down to the row the tour points at last (AI picture
    // enhancement): nothing scrolls here, and the frame generation section grew by a row (Character Hold).
    width: 1100; height: 800
    PushTour { id: tour }
    PushQuickSetup { id: quick; x: 20; y: 20; width: 560; compact: true }
    property var asked: []
    Item {
        id: surface
        anchors.fill: parent
        Rectangle { id: card; x: 640; y: 90; width: 300; height: 60; color: "transparent" }
        Rectangle { id: dialogRow; x: 640; y: 300; width: 300; height: 36; color: "transparent" }
        OnboardingGuide {
            id: guide
            parent: surface
            // As Main.qml answers: a place on the home page, a row of the capture dialog, or nothing.
            targetFor: key => tour.onHome(key) ? { item: key === "capture" ? card : quick.tourItem(key) }
                              : tour.inDialog(key) ? { item: dialogRow } : null
            onStepRequested: step => {
                const list = root.asked.slice()
                list.push(step.target)
                root.asked = list
            }
        }
    }

    TestCase {
        name: "PushTour"
        when: windowShown
        property var capabilities

        function init() {
            capabilities = veyra.effectCapabilities
            veyra.preferences = ({})
            root.asked = []
        }
        function cleanup() {
            guide.close()
            veyra.effectCapabilities = capabilities
        }
        function control(name) {
            const item = findChild(guide.contentItem, "onboarding-" + name)
            verify(item !== null, name)
            return item
        }

        function test_steps_follow_what_a_new_user_does() {
            const steps = tour.steps({ enhance: true })
            const targets = steps.map(s => s.target)
            compare(targets, ["", "capture", "device", "resolution", "capture-rate", "format", "input-color", "connect",
                              "console", "target", "enhance", "tools", ""])
            for (let i = 0; i < steps.length; ++i) {
                verify(String(steps[i].title).length > 0, "title of step " + i)
                verify(String(steps[i].detail).length > 0, "text of step " + i)
            }
            // The first and the last are cards of their own: they point at nothing.
            compare(steps[0].target, "")
            compare(steps[steps.length - 1].target, "")
            // The capture dialog's rows, in the dialog's own order; everything else with a name is on the home page.
            compare(targets.filter(t => tour.inDialog(t)), tour.dialogKeys)
            verify(tour.onHome("capture") && tour.onHome("console") && tour.onHome("target") && tour.onHome("tools"))
            verify(!tour.onHome("") && !tour.onHome("device") && !tour.inDialog("") && !tour.inDialog("capture"))
        }

        function test_a_step_for_what_this_pc_does_not_offer_is_left_out() {
            const without = tour.steps({ enhance: false }).map(s => s.target)
            compare(without.indexOf("enhance"), -1)
            compare(without.length, 12)
            compare(tour.steps(undefined).map(s => s.target).indexOf("enhance"), -1)
            compare(tour.steps({}).length, 12)
            // Nothing else depends on it: the other steps are the same and in the same order.
            compare(without, tour.steps({ enhance: true }).map(s => s.target).filter(t => t !== "enhance"))
        }

        function test_offered_by_itself_only_on_a_new_install() {
            verify(tour.offeredAtStart({}, { captureUsed: false, testRun: false }), "nothing was ever set up")
            verify(tour.offeredAtStart(undefined, undefined))
            verify(!tour.offeredAtStart({}, { testRun: true }), "never in a test or screenshot run")
            verify(!tour.offeredAtStart({ pushTourSeen: true }, {}), "once is enough")
            verify(!tour.offeredAtStart({ pushConsole: "xbox" }, {}), "a console was chosen: not a new user")
            verify(!tour.offeredAtStart({ pushGameFps: 30 }, {}))
            verify(!tour.offeredAtStart({ pushProfiles: "{}" }, {}))
            verify(!tour.offeredAtStart({}, { captureUsed: true }), "a capture card was used here before")
            // Other saved choices say nothing about being new.
            verify(tour.offeredAtStart({ language: "en", pushTheme: "black", pushConsole: "" }, {}))
        }

        function test_the_tour_walks_every_step_and_ends() {
            const steps = tour.steps({ enhance: true })
            guide.steps = steps
            guide.start()
            tryCompare(guide, "opened", true)
            compare(guide.stepIndex, 0)
            verify(!control("previous").enabled)
            for (let i = 0; i < steps.length; ++i) {
                compare(guide.stepIndex, i)
                compare(guide.step.target, steps[i].target)
                compare(control("title").text, steps[i].title)
                if (steps[i].target === "") {
                    // A card in the middle of the window, nothing lit up.
                    wait(80)
                    compare(guide.spotlight.width, 0)
                    verify(!control("spotlight").visible)
                } else tryVerify(() => guide.spotlight.width > 0 && control("spotlight").visible, 3000, "step " + i + " points at something")
                compare(control("next").text.length > 0, true)
                mouseClick(control("next"))
            }
            tryCompare(guide, "opened", false)
            compare(root.asked, steps.map(s => s.target))
            // Started again, it begins at the beginning.
            guide.start()
            tryCompare(guide, "opened", true)
            compare(guide.stepIndex, 0)
        }

        function test_leaving_early_with_escape_or_the_cross() {
            guide.steps = tour.steps({ enhance: false })
            guide.start()
            tryCompare(guide, "opened", true)
            mouseClick(control("next"))
            compare(guide.stepIndex, 1)
            tryVerify(() => guide.spotlight.width > 0, 3000, "the second step points at the capture card")
            // The cross: what it does (where it is and that it can be hit is the tour component's own test).
            control("close").clicked()
            tryCompare(guide, "opened", false)
            compare(guide.stepIndex, -1)
            guide.start()
            tryCompare(guide, "opened", true)
            compare(guide.stepIndex, 0, "it starts at the beginning again, not where it was left")
            keyClick(Qt.Key_Escape)
            tryCompare(guide, "opened", false)
        }

        function test_quick_setup_says_what_a_step_points_at() {
            verify(quick.tourItem("console") !== null)
            compare(quick.tourItem("console").sectionId, "setup")
            compare(quick.tourItem("target").objectName, "pushfps-target-row")
            compare(quick.tourItem("tools").objectName, "pushfps-tools-row")
            compare(quick.tourItem("capture"), null)       // the home page's own card, not the quick setup's
            compare(quick.tourItem("nothing"), null)
            compare(quick.tourItem(""), null)
            // AI picture enhancement is a step only where it can be switched on.
            veyra.effectCapabilities = ({ nr: { available: true } })
            verify(quick.nrPossible)
            compare(quick.tourItem("enhance").objectName, "pushfps-enhance-row")
            veyra.effectCapabilities = ({ nr: { available: false, reason: "needs another card" } })
            verify(!quick.nrPossible)
            compare(quick.tourItem("enhance"), null)
        }

        function test_a_folded_section_is_opened_for_its_step() {
            veyra.preferences = ({ pushQuickFolded: "fg,picture" })
            verify(!quick.sectionOpen("fg") && !quick.sectionOpen("picture") && quick.sectionOpen("setup"))
            quick.showTourStep("target")
            verify(quick.sectionOpen("fg"), "the section with the target row is open")
            verify(!quick.sectionOpen("picture"), "no other section is touched")
            quick.showTourStep("tools")      // the tools stand in no section
            quick.showTourStep("console")    // already open
            quick.showTourStep("")
            compare(String(veyra.preferences.pushQuickFolded), "picture")
            quick.showTourStep("enhance")
            compare(String(veyra.preferences.pushQuickFolded || ""), "")
        }
    }
}

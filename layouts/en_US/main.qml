import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components

KeyboardLayout {
    Component.onCompleted: console.log("SwipeKeyboard: main.qml loaded, inputMethod:", createInputMethod())

    function createInputMethod() {
        return Qt.createQmlObject('import Plasma.SwipeKeyboard; SwipeInputMethod {}', parent)
    }
    sharedLayouts: ['symbols']

    KeyboardRow {
        KeyboardColumn {
            Layout.preferredWidth: 1
            BackspaceKey {}
            ShiftKey {}
            HideKeyboardKey { visible: true }
            Key { key: Qt.Key_Space; text: " "; displayText: "⎵"; highlighted: true }
        }
        KeyboardColumn {
            Layout.preferredWidth: 8

            MouseArea {
                id: swipeArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                property var activeTrace: null
                property int traceId: 0
                property var inkPoints: []

                // Key positions match wordmatcher.cpp's keyCenter() table (normalized [0,1]).
                readonly property var rows: [
                    { y: 0.166, xStart: 0.05, letters: "qwertyuiop" },
                    { y: 0.500, xStart: 0.10, letters: "asdfghjkl"  },
                    { y: 0.833, xStart: 0.20, letters: "zxcvbnm"    }
                ]

                Rectangle {
                    anchors.fill: parent
                    color: "#151515"
                    radius: 6

                    Repeater {
                        model: swipeArea.rows
                        delegate: Repeater {
                            required property var modelData
                            model: modelData.letters.length
                            delegate: Rectangle {
                                id: keyTile
                                required property int index
                                readonly property string letter: modelData.letters[keyTile.index].toUpperCase()

                                width:  swipeArea.width  * 0.09
                                height: swipeArea.height * 0.28
                                x: swipeArea.width  * (modelData.xStart + keyTile.index * 0.10) - width / 2
                                y: swipeArea.height * modelData.y - height / 2

                                color: "#2c2c2c"
                                radius: 6
                                border.color: "#404040"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: keyTile.letter
                                    color: "#cfcfcf"
                                    font.pixelSize: Math.min(parent.height * 0.5, parent.width * 0.55)
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }
                }

                Shape {
                    id: inkShape
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: "#7dd3fc"
                        strokeWidth: 4
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        PathPolyline { path: swipeArea.inkPoints }
                    }
                }

                onPressed: (mouse) => {
                    traceId++
                    inkPoints = [Qt.point(mouse.x, mouse.y)]
                    var captureInfo = { channels: ['t'], sampleRate: 60, uniform: false, latency: 0.0, dpi: 96 }
                    var screenInfo = { boundingBox: Qt.rect(x, y, width, height), canvasType: "keyboard" }
                    activeTrace = InputContext.inputEngine.traceBegin(traceId, InputEngine.PatternRecognitionMode.Handwriting, captureInfo, screenInfo)
                    if (activeTrace) {
                        var idx = activeTrace.addPoint(Qt.point(mouse.x, mouse.y))
                        activeTrace.setChannelData('t', idx, Date.now())
                    }
                }
                onPositionChanged: (mouse) => {
                    if (activeTrace) {
                        inkPoints = [...inkPoints, Qt.point(mouse.x, mouse.y)]
                        var idx = activeTrace.addPoint(Qt.point(mouse.x, mouse.y))
                        activeTrace.setChannelData('t', idx, Date.now())
                    }
                }
                onReleased: {
                    if (activeTrace) {
                        activeTrace.final = true
                        InputContext.inputEngine.traceEnd(activeTrace)
                        activeTrace = null
                    }
                }
            }
        }
        KeyboardColumn {
            Layout.preferredWidth: 1
            EnterKey {}
            Key { key: Qt.Key_Period; text: "."; alternativeKeys: "<>()/\\\"'=+-_:;,.?! "; smallText: "!?"; smallTextVisible: true; highlighted: true }
            InputModeKey {}
            ChangeLanguageKey { visible: true }
        }
    }
}

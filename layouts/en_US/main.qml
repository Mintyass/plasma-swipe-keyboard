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
                // Press position (in MouseArea-local pixels). -1 means no highlight.
                // We only highlight the start tile (the key under the initial press)
                // and clear the highlight as soon as the finger has moved far enough
                // to be a swipe — otherwise every key the finger crosses would light up.
                property real pressX: -1
                property real pressY: -1
                property real pressStartX: -1
                property real pressStartY: -1
                readonly property real swipeThresholdSq: 20 * 20  // px²

                // Long-press / accent-popup state. While altMode is true the MouseArea
                // tracks finger position over altPopup instead of feeding the matcher.
                property bool altMode: false
                property string altLetters: ""
                property int altIndex: -1
                property real altX: 0
                property real altY: 0
                property real altTileWidth: 0
                property real altTileHeight: 0

                // Key positions match wordmatcher.cpp's keyCenter() table (normalized [0,1]).
                readonly property var rows: [
                    { y: 0.166, xStart: 0.05, letters: "qwertyuiop" },
                    { y: 0.500, xStart: 0.10, letters: "asdfghjkl"  },
                    { y: 0.833, xStart: 0.20, letters: "zxcvbnm"    }
                ]

                // Alt-key sets borrowed from Qt VKB's fallback/main.qml. Keys with no
                // accents map to themselves only; longPressTimer skips them.
                readonly property var altKeys: ({
                    "e": "eèéêë",
                    "r": "rř",
                    "t": "tŧť",
                    "y": "yýŷÿ",
                    "u": "uùúûüũūű",
                    "i": "iìíîïĩī",
                    "o": "oòóôõöøœ",
                    "a": "aàáâãäå",
                    "s": "sśšş",
                    "d": "dďđ",
                    "g": "gĝğģġ",
                    "l": "lľĺļłŀ",
                    "z": "zžż",
                    "c": "cćčċç",
                    "n": "nňńņ"
                })

                Timer {
                    id: longPressTimer
                    interval: 450
                    repeat: false
                    onTriggered: swipeArea.tryShowAlt()
                }

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
                                readonly property bool pressed:
                                    swipeArea.pressX >= keyTile.x
                                    && swipeArea.pressX < keyTile.x + keyTile.width
                                    && swipeArea.pressY >= keyTile.y
                                    && swipeArea.pressY < keyTile.y + keyTile.height

                                width:  swipeArea.width  * 0.09
                                height: swipeArea.height * 0.28
                                x: swipeArea.width  * (modelData.xStart + keyTile.index * 0.10) - width / 2
                                y: swipeArea.height * modelData.y - height / 2

                                color: pressed ? "#4a4a4a" : "#2c2c2c"
                                radius: 6
                                border.color: pressed ? "#7dd3fc" : "#404040"
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: 60 } }
                                Behavior on border.color { ColorAnimation { duration: 60 } }

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

                Item {
                    id: altPopup
                    visible: swipeArea.altMode
                    x: swipeArea.altX
                    y: swipeArea.altY
                    width: swipeArea.altTileWidth * Math.max(1, swipeArea.altLetters.length)
                    height: swipeArea.altTileHeight
                    z: 100

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        color: "#1c1c1c"
                        border.color: "#7dd3fc"
                        border.width: 1
                        radius: 8
                    }

                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: swipeArea.altLetters.length
                            delegate: Rectangle {
                                required property int index
                                width: swipeArea.altTileWidth
                                height: swipeArea.altTileHeight
                                color: swipeArea.altIndex === index ? "#4a4a4a" : "#2c2c2c"
                                border.color: swipeArea.altIndex === index ? "#7dd3fc" : "#404040"
                                border.width: 1
                                radius: 6
                                Text {
                                    anchors.centerIn: parent
                                    text: InputContext.uppercase
                                          ? swipeArea.altLetters[index].toUpperCase()
                                          : swipeArea.altLetters[index]
                                    color: "#ffffff"
                                    font.pixelSize: parent.height * 0.45
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }
                }

                function tileAt(px, py) {
                    var tileW = swipeArea.width * 0.09
                    var tileH = swipeArea.height * 0.28
                    for (var r = 0; r < rows.length; r++) {
                        var row = rows[r]
                        var tileY = swipeArea.height * row.y - tileH / 2
                        if (py < tileY || py >= tileY + tileH) continue
                        for (var i = 0; i < row.letters.length; i++) {
                            var tileX = swipeArea.width * (row.xStart + i * 0.10) - tileW / 2
                            if (px >= tileX && px < tileX + tileW) {
                                return { letter: row.letters[i], x: tileX, y: tileY, w: tileW, h: tileH }
                            }
                        }
                    }
                    return null
                }

                function altIndexAt(px, py) {
                    if (py < altY || py >= altY + altTileHeight) return -1
                    var idx = Math.floor((px - altX) / altTileWidth)
                    if (idx < 0 || idx >= altLetters.length) return -1
                    return idx
                }

                function tryShowAlt() {
                    var t = tileAt(pressStartX, pressStartY)
                    if (!t) return
                    var alts = altKeys[t.letter]
                    if (!alts || alts.length <= 1) return

                    // Commit prior swipe candidate + space before the accented char so
                    // "the" + long-press "e" → "the é" (matches the auto-space behavior
                    // in SwipeInputMethod::traceEnd).
                    if (InputContext.preeditText.length > 0)
                        InputContext.commit(InputContext.preeditText + " ")

                    // Orphan the in-progress trace — don't call traceEnd, otherwise the
                    // tap path commits a stray letter. The trace is owned by the input
                    // method and cleaned up at shutdown.
                    activeTrace = null
                    inkPoints = []
                    pressX = -1
                    pressY = -1

                    altLetters = alts
                    altTileWidth = t.w
                    altTileHeight = t.h
                    var totalW = altTileWidth * alts.length
                    altX = Math.max(4, Math.min(swipeArea.width - totalW - 4, t.x + t.w / 2 - totalW / 2))
                    var aboveY = t.y - altTileHeight - 8
                    altY = aboveY >= 0 ? aboveY : t.y + t.h + 8
                    altMode = true
                    altIndex = altIndexAt(pressStartX, pressStartY)
                }

                onPressed: (mouse) => {
                    traceId++
                    pressStartX = mouse.x
                    pressStartY = mouse.y
                    pressX = mouse.x
                    pressY = mouse.y
                    inkPoints = [Qt.point(mouse.x, mouse.y)]
                    var captureInfo = { channels: ['t'], sampleRate: 60, uniform: false, latency: 0.0, dpi: 96 }
                    var screenInfo = { boundingBox: Qt.rect(x, y, width, height), canvasType: "keyboard" }
                    activeTrace = InputContext.inputEngine.traceBegin(traceId, InputEngine.PatternRecognitionMode.Handwriting, captureInfo, screenInfo)
                    if (activeTrace) {
                        var idx = activeTrace.addPoint(Qt.point(mouse.x, mouse.y))
                        activeTrace.setChannelData('t', idx, Date.now())
                    }
                    longPressTimer.restart()
                }
                onPositionChanged: (mouse) => {
                    if (altMode) {
                        altIndex = altIndexAt(mouse.x, mouse.y)
                        return
                    }
                    if (activeTrace) {
                        // Once finger has moved enough to be a swipe, drop the tile
                        // highlight so we don't light up every key the trail crosses,
                        // and cancel the long-press timer (this is a swipe, not a hold).
                        if (pressX >= 0) {
                            var dx = mouse.x - pressStartX
                            var dy = mouse.y - pressStartY
                            if (dx * dx + dy * dy > swipeThresholdSq) {
                                pressX = -1
                                pressY = -1
                                longPressTimer.stop()
                            }
                        }
                        inkPoints = [...inkPoints, Qt.point(mouse.x, mouse.y)]
                        var idx = activeTrace.addPoint(Qt.point(mouse.x, mouse.y))
                        activeTrace.setChannelData('t', idx, Date.now())
                    }
                }
                onReleased: {
                    longPressTimer.stop()
                    if (altMode) {
                        if (altIndex >= 0 && altIndex < altLetters.length) {
                            var ch = altLetters[altIndex]
                            if (InputContext.uppercase) ch = ch.toUpperCase()
                            InputContext.commit(ch)
                        }
                        altMode = false
                        altIndex = -1
                        altLetters = ""
                        pressStartX = -1
                        pressStartY = -1
                        return
                    }
                    pressStartX = -1
                    pressStartY = -1
                    pressX = -1
                    pressY = -1
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

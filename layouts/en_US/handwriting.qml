// plasma-swipe-keyboard: swipe input method layout for en_US handwriting mode
import QtQuick
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components

KeyboardLayout {
    function createInputMethod() {
        return Qt.createQmlObject('import Plasma.SwipeKeyboard; SwipeInputMethod {}', parent)
    }
    sharedLayouts: ['symbols']

    KeyboardRow {
        KeyboardColumn {
            Layout.preferredWidth: 1
            InputModeKey {}
            ShiftKey {}
            HandwritingModeKey {}
            HideKeyboardKey { visible: true }
        }
        KeyboardColumn {
            Layout.preferredWidth: 8
            TraceInputKey {
                objectName: "swipeInputArea"
                patternRecognitionMode: InputEngine.PatternRecognitionMode.Handwriting
            }
        }
        KeyboardColumn {
            Layout.preferredWidth: 1
            BackspaceKey {}
            Key {
                key: Qt.Key_Period
                text: "."
                alternativeKeys: "<>()/\\\"'=+-_:;,.?! "
                smallText: "!?"
                smallTextVisible: true
                highlighted: true
            }
            EnterKey {}
            Key {
                key: Qt.Key_Space
                text: " "
                displayText: "space"
                highlighted: true
            }
        }
    }
}

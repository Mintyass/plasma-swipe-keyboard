import QtQuick
import QtQuick.Controls.Basic
import QtQuick.VirtualKeyboard

ApplicationWindow {
    id: window
    width: 800
    height: 600
    visible: true
    title: "Swipe Keyboard Test"

    TextField {
        id: field
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 40
        width: 400
        placeholderText: "Tap here, then swipe across the QWERTY keys"
        font.pixelSize: 18
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: field.bottom
        anchors.topMargin: 16
        text: "Input: " + field.text
        font.pixelSize: 14
    }

    InputPanel {
        id: inputPanel
        z: 99
        x: 0
        y: window.height
        width: window.width

        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                y: window.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            NumberAnimation { properties: "y"; duration: 250; easing.type: Easing.InOutQuad }
        }
    }
}

#pragma once

#include "wordmatcher.h"
#include <QtVirtualKeyboard/qvirtualkeyboardabstractinputmethod.h>
#include <QtVirtualKeyboard/qvirtualkeyboardtrace.h>
#include <QtVirtualKeyboard/qvirtualkeyboardinputengine.h>
#include <QtVirtualKeyboard/qvirtualkeyboardselectionlistmodel.h>
#include <QSizeF>
#include <QVariant>
#include <QList>

QT_BEGIN_NAMESPACE

class SwipeInputMethod : public QVirtualKeyboardAbstractInputMethod
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SwipeInputMethod(QObject *parent = nullptr);

    QList<QVirtualKeyboardInputEngine::InputMode> inputModes(const QString &locale) override;
    bool setInputMode(const QString &locale, QVirtualKeyboardInputEngine::InputMode inputMode) override;
    bool setTextCase(QVirtualKeyboardInputEngine::TextCase textCase) override;
    bool keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers) override;

    QList<QVirtualKeyboardSelectionListModel::Type> selectionLists() override;
    int selectionListItemCount(QVirtualKeyboardSelectionListModel::Type type) override;
    QVariant selectionListData(QVirtualKeyboardSelectionListModel::Type type, int index,
                               QVirtualKeyboardSelectionListModel::Role role) override;
    void selectionListItemSelected(QVirtualKeyboardSelectionListModel::Type type, int index) override;

    QList<QVirtualKeyboardInputEngine::PatternRecognitionMode> patternRecognitionModes() const override;
    QVirtualKeyboardTrace *traceBegin(int traceId,
                                      QVirtualKeyboardInputEngine::PatternRecognitionMode mode,
                                      const QVariantMap &traceCaptureDeviceInfo,
                                      const QVariantMap &traceScreenInfo) override;
    bool traceEnd(QVirtualKeyboardTrace *trace) override;

private:
    QString applyTextCase(const QString &s) const;

    QList<QVirtualKeyboardTrace *> m_traces;
    QStringList m_candidates;
    int m_activeIndex = -1;
    WordMatcher m_matcher;
    QSizeF m_lastTraceArea;
    QVirtualKeyboardInputEngine::TextCase m_textCase = QVirtualKeyboardInputEngine::TextCase::Lower;
};

QT_END_NAMESPACE

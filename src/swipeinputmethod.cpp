#include "swipeinputmethod.h"
#include <QtVirtualKeyboard/qvirtualkeyboardinputcontext.h>
#include <QLineF>
#include <QLoggingCategory>
#include <QRectF>

Q_LOGGING_CATEGORY(lcSwipe, "plasma.swipe")

QT_BEGIN_NAMESPACE

static void fileLog(const char *msg)
{
    FILE *f = fopen("/tmp/swipe.log", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

SwipeInputMethod::SwipeInputMethod(QObject *parent)
    : QVirtualKeyboardAbstractInputMethod(parent)
{
    fileLog("SwipeInputMethod constructed");
    qCDebug(lcSwipe) << "SwipeInputMethod created";
}

QList<QVirtualKeyboardInputEngine::InputMode> SwipeInputMethod::inputModes(const QString &)
{
    return { QVirtualKeyboardInputEngine::InputMode::Latin };
}

bool SwipeInputMethod::setInputMode(const QString &, QVirtualKeyboardInputEngine::InputMode)
{
    return true;
}

bool SwipeInputMethod::setTextCase(QVirtualKeyboardInputEngine::TextCase)
{
    return true;
}

bool SwipeInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    auto *ic = inputContext();
    if (!ic)
        return false;

    switch (key) {
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_candidates.isEmpty()) {
            ic->commit(m_candidates.first());
            m_candidates.clear();
            m_activeIndex = -1;
            emit selectionListChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList);
            emit selectionListActiveItemChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList, m_activeIndex);
        }
        return true;
    case Qt::Key_Backspace:
        // If a swipe preedit is showing, discard it instead of deleting prior text.
        if (!ic->preeditText().isEmpty()) {
            ic->clear();
            m_candidates.clear();
            m_activeIndex = -1;
            emit selectionListChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList);
            emit selectionListActiveItemChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList, m_activeIndex);
            return true;
        }
        ic->inputEngine()->virtualKeyClick(Qt::Key_Backspace, QString(), Qt::NoModifier);
        return true;
    default:
        if (!text.isEmpty()) {
            ic->inputEngine()->virtualKeyClick(key, text, Qt::NoModifier);
            return true;
        }
    }
    return false;
}

QList<QVirtualKeyboardSelectionListModel::Type> SwipeInputMethod::selectionLists()
{
    return { QVirtualKeyboardSelectionListModel::Type::WordCandidateList };
}

int SwipeInputMethod::selectionListItemCount(QVirtualKeyboardSelectionListModel::Type)
{
    return m_candidates.size();
}

QVariant SwipeInputMethod::selectionListData(QVirtualKeyboardSelectionListModel::Type,
                                              int index,
                                              QVirtualKeyboardSelectionListModel::Role role)
{
    switch (role) {
    case QVirtualKeyboardSelectionListModel::Role::Display:
        return m_candidates.at(index);
    case QVirtualKeyboardSelectionListModel::Role::WordCompletionLength:
        return 0;
    case QVirtualKeyboardSelectionListModel::Role::Dictionary:
        return static_cast<int>(QVirtualKeyboardSelectionListModel::DictionaryType::Default);
    case QVirtualKeyboardSelectionListModel::Role::CanRemoveSuggestion:
        return false;
    default:
        return QVirtualKeyboardAbstractInputMethod::selectionListData({}, index, role);
    }
}

void SwipeInputMethod::selectionListItemSelected(QVirtualKeyboardSelectionListModel::Type, int index)
{
    auto *ic = inputContext();
    if (ic && !m_candidates.isEmpty())
        ic->commit(m_candidates.at(index));
    m_candidates.clear();
    m_activeIndex = -1;
    emit selectionListChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList);
    emit selectionListActiveItemChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList, m_activeIndex);
}

QList<QVirtualKeyboardInputEngine::PatternRecognitionMode> SwipeInputMethod::patternRecognitionModes() const
{
    return { QVirtualKeyboardInputEngine::PatternRecognitionMode::Handwriting };
}

QVirtualKeyboardTrace *SwipeInputMethod::traceBegin(
        int traceId,
        QVirtualKeyboardInputEngine::PatternRecognitionMode,
        const QVariantMap &,
        const QVariantMap &traceScreenInfo)
{
    qCDebug(lcSwipe) << "traceBegin id=" << traceId;
    const QRectF bb = traceScreenInfo.value(QStringLiteral("boundingBox")).toRectF();
    if (bb.width() > 0 && bb.height() > 0)
        m_lastTraceArea = bb.size();
    auto *trace = new QVirtualKeyboardTrace(this);
    m_traces.append(trace);
    return trace;
}

bool SwipeInputMethod::traceEnd(QVirtualKeyboardTrace *trace)
{
    const QVariantList rawPoints = trace->points();
    QList<QPointF> points;
    points.reserve(rawPoints.size());
    for (const QVariant &v : rawPoints)
        points.append(v.toPointF());

    qreal traceLen = 0;
    for (int i = 1; i < points.size(); ++i)
        traceLen += QLineF(points[i - 1], points[i]).length();

    auto *ic = inputContext();

    if (!points.isEmpty() && m_lastTraceArea.isValid()
            && (points.size() < 5 || traceLen < 30.0)) {
        // Tap: route to the nearest key. Centroid in normalized [0,1] space.
        QPointF avg(0, 0);
        for (const QPointF &p : points) avg += p;
        avg /= points.size();
        const QPointF norm(avg.x() / m_lastTraceArea.width(),
                           avg.y() / m_lastTraceArea.height());
        const QChar letter = WordMatcher::nearestKey(norm);
        if (!letter.isNull() && ic && ic->inputEngine()) {
            ic->inputEngine()->virtualKeyClick(
                Qt::Key(letter.toUpper().unicode()),
                QString(letter),
                Qt::NoModifier);
        }
    } else if (m_lastTraceArea.isValid()) {
        // Swipe: run the matcher and offer candidates.
        m_candidates = m_matcher.match(points, m_lastTraceArea, 5);
        m_activeIndex = m_candidates.isEmpty() ? -1 : 0;

        FILE *f = fopen("/tmp/swipe.log", "a");
        if (f) {
            fprintf(f, "=== match: %d points, len=%.1f ===\n", points.size(), traceLen);
            for (int i = 0; i < m_candidates.size(); ++i)
                fprintf(f, "  #%d: %s\n", i, m_candidates[i].toUtf8().constData());
            fclose(f);
        }

        if (ic && !m_candidates.isEmpty())
            ic->setPreeditText(m_candidates.first());
        emit selectionListChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList);
        emit selectionListActiveItemChanged(QVirtualKeyboardSelectionListModel::Type::WordCandidateList, m_activeIndex);
    }

    m_traces.removeOne(trace);
    delete trace;
    return true;
}

QT_END_NAMESPACE

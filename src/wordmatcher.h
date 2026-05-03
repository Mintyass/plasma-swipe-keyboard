#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QPointF>
#include <QSizeF>

class WordMatcher
{
public:
    struct Weights {
        // Tuned on a 82k-word frequency dictionary with synthetic noisy traces.
        // Frequency bias (delta) is essential at this dictionary size to break ties
        // between rare lookalikes and common targets. Re-tune with real swipes.
        qreal alpha   = 0.5;   // location (point-to-point in [0,1] space)
        qreal beta    = 0.5;   // shape (centered + scale-normalized)
        qreal gamma   = 0.0;   // length deviation
        qreal delta   = 0.02;  // log-frequency bias (lowers score for common words)
        qreal epsilon = 1.5;   // endpoint anchoring (mean of start + end distance) — biggest win
    };

    WordMatcher();

    void setWeights(const Weights &w) { m_weights = w; }
    Weights weights() const { return m_weights; }

    // userPoints: trace points in trace-area-local pixels.
    // kbSize:     width/height of the trace area in pixels (used to normalize to [0,1]).
    // Returns top-N candidate words ordered best-first.
    QStringList match(const QList<QPointF> &userPoints, const QSizeF &kbSize, int topN = 5) const;

    // Returns the QWERTY letter ('a'-'z') closest to the given normalized [0,1]
    // keyboard position, or a null QChar if the table is empty.
    static QChar nearestKey(QPointF normalizedPos);

    static constexpr int RESAMPLE_N = 64;

private:
    struct Template {
        QString word;
        qreal frequency = 0;       // higher = more common
        QList<QPointF> resampled;  // N points in [0,1] keyboard-normalized space
        QList<QPointF> shape;      // resampled, then centered on origin and scaled to unit bbox
        qreal length = 0;          // path length in [0,1] space
        int dedupSize = 0;         // number of distinct key positions (after consecutive dedup)
    };

    QList<Template> m_templates;
    Weights m_weights;

    static Template buildTemplate(const QString &word, qreal frequency);
};

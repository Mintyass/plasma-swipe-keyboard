// Synthetic-trace tests for WordMatcher. No GUI, no Qt VKB; pure Qt Core.
//
// Build:  cmake --build build-release --target test_matcher
// Run:    ./build-release/test_matcher

#include "wordmatcher.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <cmath>
#include <cstdio>
#include <random>

namespace {

QPointF keyCenter(QChar c)
{
    static const QHash<QChar, QPointF> table = []() {
        QHash<QChar, QPointF> t;
        const QString row0 = QStringLiteral("qwertyuiop");
        const QString row1 = QStringLiteral("asdfghjkl");
        const QString row2 = QStringLiteral("zxcvbnm");
        for (int i = 0; i < row0.size(); ++i) t.insert(row0[i], QPointF(0.05 + i * 0.10, 0.166));
        for (int i = 0; i < row1.size(); ++i) t.insert(row1[i], QPointF(0.10 + i * 0.10, 0.500));
        for (int i = 0; i < row2.size(); ++i) t.insert(row2[i], QPointF(0.20 + i * 0.10, 0.833));
        return t;
    }();
    return table.value(c.toLower(), QPointF(-1, -1));
}

QList<QPointF> syntheticTrace(const QString &word, qreal kbW, qreal kbH,
                               int pointsPerSegment, qreal noisePx, std::mt19937 &rng)
{
    QList<QPointF> keys;
    for (QChar c : word) {
        QPointF k = keyCenter(c);
        if (k.x() < 0) return {};
        keys.append(k);
    }
    QList<QPointF> dedup;
    for (const QPointF &k : keys)
        if (dedup.isEmpty() || dedup.last() != k)
            dedup.append(k);
    if (dedup.size() < 2) return {};

    auto jitter = [&]() -> qreal {
        if (noisePx <= 0) return 0.0;
        std::normal_distribution<qreal> d(0.0, noisePx);
        return d(rng);
    };

    QList<QPointF> trace;
    for (int i = 1; i < dedup.size(); ++i) {
        for (int j = 0; j < pointsPerSegment; ++j) {
            qreal t = qreal(j) / pointsPerSegment;
            qreal x = (dedup[i - 1].x() + t * (dedup[i].x() - dedup[i - 1].x())) * kbW;
            qreal y = (dedup[i - 1].y() + t * (dedup[i].y() - dedup[i - 1].y())) * kbH;
            x += jitter(); y += jitter();
            trace.append(QPointF(x, y));
        }
    }
    trace.append(QPointF(dedup.last().x() * kbW, dedup.last().y() * kbH));
    return trace;
}

const QStringList kTestWords = {
    "the", "you", "and", "that", "this", "from", "with", "have", "for", "what",
    "but", "are", "they", "one", "out", "your", "when", "use", "word", "how",
    "said", "can", "all", "were", "some", "there", "had", "his", "was", "do",
    // common longer words
    "hello", "world", "quick", "brown", "people", "would", "could", "should",
    "first", "after", "where", "right", "thanks", "before",
    // long ambiguous words (real-world failure cases)
    "spokesperson", "trombone", "system", "different", "important", "remember",
    "everything", "themselves"
};

struct Stats { int top1 = 0; int top3 = 0; int top5 = 0; int miss = 0; qint64 totalUs = 0; int n = 0; };

Stats evaluate(WordMatcher &m, qreal noise, int trials, qreal kbW, qreal kbH, bool verbose = false)
{
    Stats stats;
    std::mt19937 rng(42);
    QElapsedTimer timer;
    for (const QString &word : kTestWords) {
        for (int trial = 0; trial < trials; ++trial) {
            auto trace = syntheticTrace(word, kbW, kbH, 20, noise, rng);
            if (trace.isEmpty()) continue;
            timer.restart();
            QStringList top = m.match(trace, QSizeF(kbW, kbH), 5);
            stats.totalUs += timer.nsecsElapsed() / 1000;
            ++stats.n;
            int rank = int(top.indexOf(word));
            if (rank == 0) ++stats.top1;
            if (rank >= 0 && rank < 3) ++stats.top3;
            if (rank >= 0 && rank < 5) ++stats.top5;
            if (rank < 0) ++stats.miss;
            if (verbose) {
                const char *mark = (rank == 0) ? "OK  " : (rank > 0 ? "ok  " : "MISS");
                printf("  [%s] \"%s\" rank=%d  top5=[", mark, qPrintable(word), rank);
                for (int i = 0; i < top.size(); ++i)
                    printf("%s%s", i ? ", " : "", qPrintable(top[i]));
                printf("]\n");
            }
        }
    }
    return stats;
}

void printStats(const char *label, const Stats &s)
{
    qint64 perMatchUs = s.n ? s.totalUs / s.n : 0;
    printf("%s: top1=%d/%d (%.0f%%)  top3=%d/%d  top5=%d/%d  miss=%d/%d   match=%lldus\n",
           label, s.top1, s.n, s.n ? 100.0 * s.top1 / s.n : 0.0,
           s.top3, s.n, s.top5, s.n, s.miss, s.n, perMatchUs);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    constexpr qreal kbW = 500.0;
    constexpr qreal kbH = 180.0;
    const int trials = 5;

    QElapsedTimer loadTimer;
    loadTimer.start();
    WordMatcher m;
    qint64 loadMs = loadTimer.elapsed();
    printf("=== loaded matcher in %lld ms ===\n\n", loadMs);

    printf("=== weights: alpha=0.5 beta=0.5 gamma=0 delta=0.02 epsilon=1.5 ===\n");
    Stats s0 = evaluate(m, 0.0, 1, kbW, kbH, true);
    printStats("ideal     ", s0);
    for (qreal n : {2.0, 4.0, 8.0, 12.0}) {
        char label[32];
        snprintf(label, sizeof(label), "noise=%-3.0fpx", n);
        Stats st = evaluate(m, n, trials, kbW, kbH);
        printStats(label, st);
    }

    return 0;
}

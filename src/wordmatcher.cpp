#include "wordmatcher.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QLineF>
#include <QLoggingCategory>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(lcMatcher, "plasma.swipe.matcher")

namespace {

struct KeyEntry { QChar c; QPointF pos; };

// Standard QWERTY centers in [0, 1] x [0, 1] normalized keyboard space.
// Row spacing matches a real QWERTY: 10 / 9 / 7 keys, 0.10 horizontal step.
const QList<KeyEntry> &keyTable()
{
    static const QList<KeyEntry> table = []() {
        QList<KeyEntry> t;
        const QString row0 = QStringLiteral("qwertyuiop");
        const QString row1 = QStringLiteral("asdfghjkl");
        const QString row2 = QStringLiteral("zxcvbnm");
        for (int i = 0; i < row0.size(); ++i) t.append({row0[i], QPointF(0.05 + i * 0.10, 0.166)});
        for (int i = 0; i < row1.size(); ++i) t.append({row1[i], QPointF(0.10 + i * 0.10, 0.500)});
        for (int i = 0; i < row2.size(); ++i) t.append({row2[i], QPointF(0.20 + i * 0.10, 0.833)});
        return t;
    }();
    return table;
}

QPointF keyCenter(QChar c)
{
    static const QHash<QChar, QPointF> hash = []() {
        QHash<QChar, QPointF> h;
        for (const KeyEntry &e : keyTable())
            h.insert(e.c, e.pos);
        return h;
    }();
    return hash.value(c.toLower(), QPointF(-1, -1));
}

qreal pathLength(const QList<QPointF> &pts)
{
    qreal len = 0;
    for (int i = 1; i < pts.size(); ++i)
        len += QLineF(pts[i - 1], pts[i]).length();
    return len;
}

QList<QPointF> dedupeConsecutive(const QList<QPointF> &pts)
{
    QList<QPointF> out;
    out.reserve(pts.size());
    for (const QPointF &p : pts) {
        if (out.isEmpty() || out.last() != p)
            out.append(p);
    }
    return out;
}

// Wobbrock $1 style arc-length resampling to N equidistant points.
QList<QPointF> resample(const QList<QPointF> &raw, int N)
{
    if (raw.size() < 2 || N < 2)
        return raw;

    const qreal totalLen = pathLength(raw);
    if (totalLen <= 0)
        return QList<QPointF>(N, raw.first());

    const qreal interval = totalLen / (N - 1);
    QList<QPointF> pts(raw); // mutable copy; resampled points are inserted as we go
    QList<QPointF> out;
    out.reserve(N);
    out.append(pts.first());

    qreal accumulated = 0;
    for (int i = 1; i < pts.size(); ++i) {
        const qreal d = QLineF(pts[i - 1], pts[i]).length();
        if (accumulated + d >= interval) {
            const qreal t = (interval - accumulated) / d;
            const QPointF q(pts[i - 1].x() + t * (pts[i].x() - pts[i - 1].x()),
                            pts[i - 1].y() + t * (pts[i].y() - pts[i - 1].y()));
            out.append(q);
            pts.insert(i, q);
            accumulated = 0;
        } else {
            accumulated += d;
        }
    }
    while (out.size() < N)
        out.append(pts.last());
    if (out.size() > N)
        out.resize(N);
    return out;
}

QList<QPointF> centerAndScale(const QList<QPointF> &pts)
{
    if (pts.isEmpty())
        return {};
    qreal minX = pts.first().x(), maxX = minX;
    qreal minY = pts.first().y(), maxY = minY;
    for (const QPointF &p : pts) {
        if (p.x() < minX) minX = p.x();
        if (p.x() > maxX) maxX = p.x();
        if (p.y() < minY) minY = p.y();
        if (p.y() > maxY) maxY = p.y();
    }
    const qreal cx = (minX + maxX) / 2;
    const qreal cy = (minY + maxY) / 2;
    qreal scale = qMax(maxX - minX, maxY - minY);
    if (scale < 1e-9)
        scale = 1;
    QList<QPointF> out;
    out.reserve(pts.size());
    for (const QPointF &p : pts)
        out.append(QPointF((p.x() - cx) / scale, (p.y() - cy) / scale));
    return out;
}

qreal meanPairDistance(const QList<QPointF> &a, const QList<QPointF> &b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 1e9;
    qreal sum = 0;
    for (int i = 0; i < a.size(); ++i)
        sum += QLineF(a[i], b[i]).length();
    return sum / a.size();
}

// Counts sharp direction changes in a resampled (equidistant) trace.
// Used to estimate the number of distinct keys the user visited so we can
// pre-filter the dictionary to words of similar length. The window controls
// the smoothing radius (resists jitter); the debounce stride controls the
// minimum spacing between detected corners.
int countCorners(const QList<QPointF> &pts, int window = 8)
{
    if (pts.size() < 2 * window + 1)
        return 0;
    const int debounce = qMax(2, window / 2);
    int corners = 0;
    int lastCornerIdx = -debounce * 2;
    for (int i = window; i + window < pts.size(); ++i) {
        const QPointF a = pts[i] - pts[i - window];
        const QPointF b = pts[i + window] - pts[i];
        const qreal la = std::hypot(a.x(), a.y());
        const qreal lb = std::hypot(b.x(), b.y());
        if (la < 1e-9 || lb < 1e-9) continue;
        const qreal dot = (a.x() * b.x() + a.y() * b.y()) / (la * lb);
        if (dot < 0.3 && i - lastCornerIdx > debounce) { // > ~73° turn, debounced
            ++corners;
            lastCornerIdx = i;
        }
    }
    return corners;
}

// Returns true if `word` is purely lowercase ASCII a-z.
bool isLowerAscii(QStringView word)
{
    if (word.isEmpty())
        return false;
    for (QChar c : word) {
        const ushort u = c.unicode();
        if (u < 'a' || u > 'z')
            return false;
    }
    return true;
}

constexpr int kMinLen = 2;
constexpr int kMaxLen = 20;

} // namespace

WordMatcher::WordMatcher()
{
    QElapsedTimer timer;
    timer.start();

    QFile file(QStringLiteral(":/swipe/frequency_dictionary_en_82_765.txt"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcMatcher) << "WordMatcher: cannot open dictionary resource"
                             << file.errorString();
        return;
    }

    int parsed = 0, kept = 0, skipped = 0;
    while (!file.atEnd()) {
        QByteArray rawLine = file.readLine();
        // Strip UTF-8 BOM on first line.
        if (parsed == 0 && rawLine.startsWith("\xEF\xBB\xBF"))
            rawLine.remove(0, 3);
        ++parsed;

        // Lines look like: "word 12345\n". Single space separator.
        const int sp = rawLine.indexOf(' ');
        if (sp <= 0) { ++skipped; continue; }

        const QString word = QString::fromLatin1(rawLine.left(sp)).toLower();
        bool ok = false;
        const qreal freq = QByteArray(rawLine.constData() + sp + 1).trimmed().toDouble(&ok);
        if (!ok || freq <= 0) { ++skipped; continue; }

        if (word.size() < kMinLen || word.size() > kMaxLen) { ++skipped; continue; }
        if (!isLowerAscii(word)) { ++skipped; continue; }

        Template t = buildTemplate(word, freq);
        if (!t.resampled.isEmpty()) {
            m_templates.append(std::move(t));
            ++kept;
        } else {
            ++skipped;
        }
    }

    qCInfo(lcMatcher) << "WordMatcher: loaded" << kept << "of" << parsed
                      << "(skipped" << skipped << ") in" << timer.elapsed() << "ms";
}

WordMatcher::Template WordMatcher::buildTemplate(const QString &word, qreal frequency)
{
    Template t;
    t.word = word;
    t.frequency = frequency;

    QList<QPointF> keys;
    keys.reserve(word.size());
    for (QChar c : word) {
        const QPointF k = keyCenter(c);
        if (k.x() < 0)
            return {}; // unknown letter -> skip word
        keys.append(k);
    }
    keys = dedupeConsecutive(keys);
    t.dedupSize = keys.size();

    if (keys.size() < 2) {
        // Single-key word collapsed to one position. Template is that point repeated.
        const QPointF p = keys.first();
        t.resampled = QList<QPointF>(RESAMPLE_N, p);
        t.shape = QList<QPointF>(RESAMPLE_N, QPointF(0, 0));
        t.length = 0;
        return t;
    }

    t.resampled = resample(keys, RESAMPLE_N);
    t.shape = centerAndScale(t.resampled);
    t.length = pathLength(t.resampled);
    return t;
}

QStringList WordMatcher::match(const QList<QPointF> &userPoints,
                                const QSizeF &kbSize,
                                int topN) const
{
    if (userPoints.size() < 2 || kbSize.width() <= 0 || kbSize.height() <= 0)
        return {};

    // Normalize user trace into [0, 1] x [0, 1] keyboard space.
    QList<QPointF> norm;
    norm.reserve(userPoints.size());
    for (const QPointF &p : userPoints)
        norm.append(QPointF(p.x() / kbSize.width(), p.y() / kbSize.height()));

    const QList<QPointF> userRes   = resample(norm, RESAMPLE_N);
    const QList<QPointF> userShape = centerAndScale(userRes);
    const qreal          userLen   = pathLength(userRes);

    // Length pre-filter: estimate the number of keys visited from the trace's
    // direction-change count, then drop templates whose deduped key count is far off.
    // letters ≈ corners + 2 (start, each interior corner, end).
    const int corners = countCorners(userRes);
    const int expectedKeys = corners + 2;
    constexpr int kKeyTolerance = 7;

    QList<std::pair<qreal, QString>> scored;
    scored.reserve(m_templates.size() / 4);
    const QPointF userStart = userRes.first();
    const QPointF userEnd   = userRes.last();
    for (const Template &t : m_templates) {
        if (qAbs(t.dedupSize - expectedKeys) > kKeyTolerance)
            continue;
        const qreal location = meanPairDistance(userRes,   t.resampled);
        const qreal shape    = meanPairDistance(userShape, t.shape);
        const qreal length   = (t.length > 0) ? qAbs(userLen - t.length) / t.length : 0;
        const qreal startDist = QLineF(userStart, t.resampled.first()).length();
        const qreal endDist   = QLineF(userEnd,   t.resampled.last()).length();
        const qreal endpoint  = (startDist + endDist) * 0.5;
        const qreal freqBias = std::log(t.frequency + 1.0);
        const qreal s        = m_weights.alpha   * location
                             + m_weights.beta    * shape
                             + m_weights.gamma   * length
                             + m_weights.epsilon * endpoint
                             - m_weights.delta   * freqBias;
        scored.append({s, t.word});
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    QStringList out;
    const int n = std::min(topN, int(scored.size()));
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.append(scored[i].second);
    return out;
}

QChar WordMatcher::nearestKey(QPointF p)
{
    const auto &table = keyTable();
    if (table.isEmpty())
        return {};
    qreal bestDist = std::numeric_limits<qreal>::max();
    QChar best;
    for (const KeyEntry &e : table) {
        const qreal dx = p.x() - e.pos.x();
        const qreal dy = p.y() - e.pos.y();
        const qreal d  = dx * dx + dy * dy;
        if (d < bestDist) { bestDist = d; best = e.c; }
    }
    return best;
}

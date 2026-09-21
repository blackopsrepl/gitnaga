#include "graph_palette.hpp"

#include <QTest>

#include <array>
#include <cmath>

// Colour contracts for the lane palette, kept out of the geometry test so each
// file owns one concern and stays well inside the source-size cap.

using namespace GitNaga;
using namespace GitNaga::graph;
namespace {

// CIELab distance, so the palette contract is expressed in perceptual units
// rather than in raw channel deltas.
std::array<double, 3> labOf(const QColor &color)
{
    const auto toLab = [](const QColor &color) {
        const auto linear = [](double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        const double r = linear(color.redF());
        const double g = linear(color.greenF());
        const double b = linear(color.blueF());
        const double x = (r * 0.4124564 + g * 0.3575761 + b * 0.1804375) / 0.95047;
        const double y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
        const double z = (r * 0.0193339 + g * 0.1191920 + b * 0.9503041) / 1.08883;
        const auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0; };
        return std::array<double, 3>{ 116.0 * f(y) - 16.0, 500.0 * (f(x) - f(y)), 200.0 * (f(y) - f(z)) };
    };
    return toLab(color);
}

double labDistance(const QColor &first, const QColor &second)
{
    const auto a = labOf(first);
    const auto b = labOf(second);
    return std::sqrt(std::pow(a[0] - b[0], 2) + std::pow(a[1] - b[1], 2) + std::pow(a[2] - b[2], 2));
}

double contrastOnBackground(const QColor &color)
{
    const auto luminance = [](const QColor &c) {
        const auto linear = [](double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(c.redF()) + 0.7152 * linear(c.greenF()) + 0.0722 * linear(c.blueF());
    };
    const QColor background(QStringLiteral("#080b12"));
    return (luminance(color) + 0.05) / (luminance(background) + 0.05);
}

int hueOf(const QColor &color)
{
    return color.hsvHue();
}

} // namespace
class PaletteTest final : public QObject
{
    Q_OBJECT

private slots:
    void paletteStaysDistinguishable()
    {
        // Regression: the earlier palette held two greens at dE 20 and two
        // violets at dE 21, so distinct branches read as the same colour.
        const auto &colors = palette();
        double closest = 1e9;
        for (std::size_t i = 0; i < colors.size(); ++i) {
            for (std::size_t j = i + 1; j < colors.size(); ++j)
                closest = std::min(closest, labDistance(colors.at(i), colors.at(j)));
        }
        QVERIFY2(closest >= 24.0, qPrintable(QStringLiteral("closest pair dE %1").arg(closest)));

        // Adjacent indices are the lanes a reader compares side by side.
        double closestNeighbour = 1e9;
        for (std::size_t i = 0; i + 1 < colors.size(); ++i)
            closestNeighbour = std::min(closestNeighbour, labDistance(colors.at(i), colors.at(i + 1)));
        QVERIFY2(closestNeighbour >= 80.0,
                 qPrintable(QStringLiteral("closest neighbours dE %1").arg(closestNeighbour)));

        // Thin strokes on the near-black window must stay legible, the first
        // branch must lead with green (red is reserved for destructive
        // actions), the set must lead with green overall, and it may not carry
        // more than one violet entry.
        int greens = 0;
        int violets = 0;
        for (const auto &color : colors) {
            QVERIFY2(contrastOnBackground(color) >= 4.0,
                     qPrintable(QStringLiteral("%1 contrast %2").arg(color.name(),
                                                                    QString::number(contrastOnBackground(color)))));
            const int hue = hueOf(color);
            if (hue >= 70 && hue <= 185)
                ++greens;
            if (hue >= 250 && hue <= 300)
                ++violets;
        }
        QVERIFY2(greens >= 3, qPrintable(QStringLiteral("green entries %1").arg(greens)));
        QVERIFY(violets <= 1);
        const int firstHue = hueOf(colors.front());
        QVERIFY2(firstHue >= 70 && firstHue <= 185,
                 qPrintable(QStringLiteral("first entry hue %1 is not green").arg(firstHue)));
    }

    void pointerCueStaysNeutral()
    {
        // The pointer highlight must not carry a hue: deriving it from the
        // branch colour made hovering the green line look like a green state.
        const auto pointer = labOf(pointerColor());
        QVERIFY2(pointer[0] >= 80.0, qPrintable(QStringLiteral("pointer L* %1 too dark").arg(pointer[0])));
        const double chroma = std::hypot(pointer[1], pointer[2]);
        QVERIFY2(chroma <= 10.0,
                 qPrintable(QStringLiteral("pointer chroma %1 is not neutral").arg(chroma)));
        QVERIFY(contrastOnBackground(pointerColor()) >= 8.0);

        // Branch colours, by contrast, are required to be chromatic, so the
        // pointer and the identity can never converge on the same treatment.
        for (const auto &color : palette()) {
            const auto branch = labOf(color);
            QVERIFY(std::hypot(branch[1], branch[2]) >= 25.0);
        }
    }

    void paletteWrapsAndStaysDistinct()
    {
        QVERIFY(laneColor(0).isValid());
        QCOMPARE(laneColor(0), laneColor(static_cast<int>(palette().size())));
        QVERIFY(laneColor(0) != laneColor(1));
        QVERIFY(laneColor(-1).isValid());
        QVERIFY(lighten(laneColor(0), 0.5).lightnessF() > laneColor(0).lightnessF());
        QVERIFY(darken(laneColor(0), 0.5).lightnessF() < laneColor(0).lightnessF());
        QCOMPARE(withAlpha(laneColor(0), 12).alpha(), 12);
    }
};

QTEST_GUILESS_MAIN(PaletteTest)

#include "palette_test.moc"

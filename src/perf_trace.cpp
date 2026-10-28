#include "perf_trace.hpp"

#include <QByteArray>
#include <QSet>
#include <QtGlobal>

#include <cstdio>

namespace GitNaga::perf {
namespace {

QElapsedTimer &clock()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer started;
        started.start();
        return started;
    }();
    return timer;
}

QSet<QByteArray> &reported()
{
    static QSet<QByteArray> once;
    return once;
}

} // namespace

bool enabled()
{
    static const bool on = qEnvironmentVariableIsSet("GITNAGA_PERF");
    return on;
}

void restart()
{
    if (!enabled())
        return;
    clock().restart();
    reported().clear();
}

void mark(const char *milestone)
{
    if (!enabled())
        return;
    std::fprintf(stderr, "perf: %8.1f ms  %s\n", static_cast<double>(clock().nsecsElapsed()) / 1e6, milestone);
}

void markOnce(const char *milestone)
{
    if (!enabled() || reported().contains(QByteArray(milestone)))
        return;
    reported().insert(QByteArray(milestone));
    mark(milestone);
}

Scope::Scope(const char *name, qint64 budgetMicroseconds)
    : m_name(name)
    , m_budget(budgetMicroseconds)
{
    if (enabled())
        m_timer.start();
}

Scope::~Scope()
{
    if (!enabled())
        return;
    const auto elapsed = m_timer.nsecsElapsed() / 1000;
    if (elapsed >= m_budget)
        std::fprintf(stderr, "perf: slow %s %.2f ms\n", m_name, static_cast<double>(elapsed) / 1000.0);
}

} // namespace GitNaga::perf

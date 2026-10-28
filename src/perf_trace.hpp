#pragma once

#include <QElapsedTimer>

namespace GitNaga::perf {

// Opt-in milestones for measuring a huge history: with GITNAGA_PERF set, each
// mark prints the milliseconds since the last open request to stderr. Off by
// default and a single branch when off, so it can stay in the hot paths.
bool enabled();
void restart();
void mark(const char *milestone);
// A milestone that should be reported once per open (the first painted rows).
void markOnce(const char *milestone);

// Measures one block of GUI-thread work and reports it when it exceeds a frame
// budget, so a stall shows up by name instead of as a vague stutter.
class Scope
{
public:
    explicit Scope(const char *name, qint64 budgetMicroseconds = 4000);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

private:
    const char *m_name;
    qint64 m_budget;
    QElapsedTimer m_timer;
};

} // namespace GitNaga::perf

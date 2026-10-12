#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/*
 * CriticalSection / ScopedLock
 * Minimal RAII lock (the v100 toolset has no std::mutex). Used for module state
 * that engine hooks may touch from a loader or worker thread while the main
 * loop reads it.
 */

namespace TotalRestore
{
    class CriticalSection
    {
    public:
        CriticalSection() { InitializeCriticalSection(&m_section); }
        ~CriticalSection() { DeleteCriticalSection(&m_section); }
        void Lock() { EnterCriticalSection(&m_section); }
        void Unlock() { LeaveCriticalSection(&m_section); }

    private:
        CriticalSection(const CriticalSection&);
        CriticalSection& operator=(const CriticalSection&);
        CRITICAL_SECTION m_section;
    };

    class ScopedLock
    {
    public:
        explicit ScopedLock(CriticalSection& section) : m_section(section) { m_section.Lock(); }
        ~ScopedLock() { m_section.Unlock(); }

    private:
        ScopedLock(const ScopedLock&);
        ScopedLock& operator=(const ScopedLock&);
        CriticalSection& m_section;
    };
}

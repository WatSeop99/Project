#include "../pch.h"
#include "Timer.h"

Timer::Timer()
{
    QueryPerformanceFrequency(&m_QPCFrequency);
    QueryPerformanceCounter(&m_QPCLastTime);

    // Initialize max delta to 1/10 of a second.
    m_QPCMaxDelta = m_QPCFrequency.QuadPart / 10;
}

void Timer::ResetElapsedTime()
{
    QueryPerformanceCounter(&m_QPCLastTime);

    m_LeftOverTicks = 0;
    m_FramesPerSecond = 0;
    m_FramesThisSecond = 0;
    m_QPCSecondCounter = 0;
}

void Timer::Tick(LPUPDATEFUNC update)
{
    LARGE_INTEGER currentTime;

    QueryPerformanceCounter(&currentTime);

    UINT64 timeDelta = currentTime.QuadPart - m_QPCLastTime.QuadPart;

    m_QPCLastTime = currentTime;
    m_QPCSecondCounter += timeDelta;

    // Clamp excessively large time deltas (e.g. after paused in the debugger).
    if (timeDelta > m_QPCMaxDelta)
    {
        timeDelta = m_QPCMaxDelta;
    }

    // Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
    timeDelta *= s_TICKS_PER_SECOND;
    timeDelta /= m_QPCFrequency.QuadPart;

    UINT32 lastFrameCount = m_FrameCount;

    if (m_bIsFixedTimeStep)
    {
        // Fixed timestep update logic

        // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
        // the clock to exactly match the target value. This prevents tiny and irrelevant errors
        // from accumulating over time. Without this clamping, a game that requested a 60 fps
        // fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
        // accumulate enough tiny errors that it would drop a frame. It is better to just round 
        // small deviations down to zero to leave things running smoothly.

        if (abs((int)(timeDelta - m_TargetElapsedTicks)) < s_TICKS_PER_SECOND / 4000)
        {
            timeDelta = m_TargetElapsedTicks;
        }

        m_LeftOverTicks += timeDelta;

        while (m_LeftOverTicks >= m_TargetElapsedTicks)
        {
            m_ElapsedTicks = m_TargetElapsedTicks;
            m_TotalTicks += m_TargetElapsedTicks;
            m_LeftOverTicks -= m_TargetElapsedTicks;
            ++m_FrameCount;

            if (update)
            {
                update();
            }
        }
    }
    else
    {
        // Variable timestep update logic.
        m_ElapsedTicks = timeDelta;
        m_TotalTicks += timeDelta;
        m_LeftOverTicks = 0;
        ++m_FrameCount;

        if (update)
        {
            update();
        }
    }

    // Track the current framerate.
    if (m_FrameCount != lastFrameCount)
    {
        ++m_FramesThisSecond;
    }

    if (m_QPCSecondCounter >= (UINT64)m_QPCFrequency.QuadPart)
    {
        m_FramesPerSecond = m_FramesThisSecond;
        m_FramesThisSecond = 0;
        m_QPCSecondCounter %= m_QPCFrequency.QuadPart;
    }
}

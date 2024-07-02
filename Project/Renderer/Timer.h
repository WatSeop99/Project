#pragma once

class Timer
{
public:
	Timer();
	~Timer() = default;

	inline UINT64 GetElapsedTicks() { return m_ElapsedTicks; }
	inline double GetElapsedSeconds() { return TicksToSeconds(m_ElapsedTicks); }

	inline UINT64 GetTotalTicks() { return m_TotalTicks; }
	inline double GetTotalSeconds() { return TicksToSeconds(m_TotalTicks); }

	inline UINT GetFrameCount() { return m_FrameCount; }

	inline UINT GetFramesPerSecond() { return m_FramesPerSecond; }

	inline void SetFixedTimeStep(bool bIsFixedTimestep) { m_bIsFixedTimeStep = bIsFixedTimestep; }

	inline void SetTargetElapsedTicks(UINT64 targetElapsed) { m_TargetElapsedTicks = targetElapsed; }
	inline void SetTargetElapsedSeconds(double targetElapsed) { m_TargetElapsedTicks = SecondsToTicks(targetElapsed); }

	void ResetElapsedTime();

	typedef void(*LPUPDATEFUNC) (void);
	void Tick(LPUPDATEFUNC update = nullptr);

public:
	static inline double TicksToSeconds(UINT64 ticks) { return (double)ticks / s_TICKS_PER_SECOND; }
	static inline UINT64 SecondsToTicks(double seconds) { return (UINT64)(seconds * s_TICKS_PER_SECOND); }

public:
	static const UINT64 s_TICKS_PER_SECOND = 10000000;

protected:
	LARGE_INTEGER m_QPCFrequency;
	LARGE_INTEGER m_QPCLastTime;
	UINT64 m_QPCMaxDelta;

	UINT64 m_ElapsedTicks = 0;
	UINT64 m_TotalTicks = 0;
	UINT64 m_LeftOverTicks = 0;

	bool m_bIsFixedTimeStep = false;

	UINT m_FrameCount = 0;
	UINT m_FramesPerSecond = 0;
	UINT m_FramesThisSecond = 0;
	UINT64 m_QPCSecondCounter = 0;

	UINT64 m_TargetElapsedTicks = s_TICKS_PER_SECOND / 60;
};

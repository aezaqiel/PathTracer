#pragma once

#include "Types.hpp"

namespace PathTracer {

    class Timer
    {
    public:
        using FloatDuration = std::chrono::duration<f32>;

        Timer() noexcept
            :   m_StartTime(Clock::now()),
                m_LastFrameTime(m_StartTime),
                m_TotalTime(FloatDuration::zero()),
                m_DeltaTime(FloatDuration::zero()),
                m_TimeScale(1.0f)
        {
        }

        inline void Tick() noexcept
        {
            const TimePoint currentTime = Clock::now();

            m_DeltaTime = currentTime - m_LastFrameTime;
            m_TotalTime = currentTime - m_StartTime;

            m_LastFrameTime = currentTime;
        }

        inline void SetTimeScale(std::floating_point auto scale) noexcept
        {
            m_TimeScale = scale;
        }

        [[nodiscard]] inline f32 GetTimeScale() const noexcept
        {
            return m_TimeScale;
        }

        [[nodiscard]] inline f32 GetDeltaTime() const noexcept
        {
            return m_DeltaTime.count();
        }

        [[nodiscard]] inline f32 GetScaledDeltaTime() const noexcept
        {
            return m_DeltaTime.count() * m_TimeScale;
        }

        [[nodiscard]] inline f32 GetTotalTime() const noexcept
        {
            return m_TotalTime.count();
        }

    private:
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TimePoint m_StartTime;
        TimePoint m_LastFrameTime;

        FloatDuration m_TotalTime;
        FloatDuration m_DeltaTime;

        f32 m_TimeScale;
    };

}

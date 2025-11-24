#ifndef TIME_H
#define TIME_H
#include <chrono>

class Time
{
public:
    static Time& Instance()
    {
        static Time instance;
        return instance;
    }

    // Call once per frame
    void Update()
    {
        auto now = std::chrono::steady_clock::now();
        m_deltaTime = std::chrono::duration<double>(now - m_lastTime).count();
        m_runningTime = std::chrono::duration<double>(now - m_startTime).count();
        m_lastTime = now;
    }

    // Time since app started (monotonic)
    double RunningTimeSeconds() const { return m_runningTime; }
    double RunningTimeMilliseconds() const { return m_runningTime * 1000.0; }

    // Time between frames
    double DeltaTimeSeconds() const { return m_deltaTime; }
    double DeltaTimeMilliseconds() const { return m_deltaTime * 1000.0; }

    // Wall clock time (system time) when app launched
    std::chrono::system_clock::time_point AppStartWallClockTime() const
    {
        return m_wallClockStartTime;
    }

    // Convenience: get app start in seconds since UNIX epoch
    double AppStartEpochSeconds() const
    {
        return std::chrono::duration<double>(m_wallClockStartTime.time_since_epoch()).count();
    }

private:
    Time()
    {
        m_startTime = std::chrono::steady_clock::now();
        m_lastTime = m_startTime;
        m_wallClockStartTime = std::chrono::system_clock::now(); // Save the actual system time
    }

    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;

    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastTime;
    std::chrono::system_clock::time_point m_wallClockStartTime;

    double m_deltaTime = 0.0;
    double m_runningTime = 0.0;
};
#endif // TIME_H

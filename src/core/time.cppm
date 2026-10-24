module;
#include <algorithm>
export module crumb.time;
import crumb.type;

export namespace crumb {

// Small clock with fixed-step accumulator for _fixed_update.
class Time {
private:
  i32 m_maxSteps = 0;
  ui64 m_frameCount = 0;
  bool m_paused = false;
  double m_fixedDelta = 0.0;
  double m_maxFrameDelta = 0.0;
  double m_accumulator = 0.0;
  double m_totalTime = 0.0;

public:
  explicit Time(double fixed_dt = 1.0 / 60.0, double max_frame = 0.25, i32 max_steps = 5)
    : m_fixedDelta(fixed_dt), m_maxFrameDelta(max_frame), m_maxSteps(max_steps) {}

  // Clamp the raw frame time and add it to the fixed-step accumulator.
  double Tick(double raw_dt) {
    if (m_paused)
      return 0.0;

    double dt = std::min(raw_dt, m_maxFrameDelta);
    m_accumulator += dt;
    m_totalTime += dt;
    m_frameCount++;

    return dt;
  }

  // Call update() at most maxSteps times to catch up on fixed steps.
  template <typename Fn>
  void ConsumeFixed(Fn&& update) {
    if (m_paused)
      return;

    int steps = 0;
    while (m_accumulator >= m_fixedDelta && steps < m_maxSteps) {
      update(m_fixedDelta);
      m_accumulator -= m_fixedDelta;
      ++steps;
    }

    if (steps == m_maxSteps)
      m_accumulator = 0.0;
  }

  inline double GetTotalTime() const { return m_totalTime; }
  inline ui64 GetFrameCount() const { return m_frameCount; }

  double Alpha() { return m_accumulator / m_fixedDelta; }

  bool IsPaused() const { return m_paused; }
  void Paused() { m_paused = true; }
  void Resume() { m_paused = false; }
  void SetPaused(bool value) { m_paused = value; }
};

}

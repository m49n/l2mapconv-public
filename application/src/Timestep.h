#pragma once

class Timestep {
public:
  explicit Timestep(float time) : m_time{time} {}

  operator float() const { return m_time; }

  auto seconds() const -> float { return m_time; }
  auto milliseconds() const -> float { return m_time * 1000.0f; }

private:
  float m_time;
};

#pragma once

#include "System.h"

#include <cstddef>
#include <memory>
#include <vector>

class SystemStack {
public:
  SystemStack() = default;
  ~SystemStack();

  SystemStack(const SystemStack &) = delete;
  auto operator=(const SystemStack &) -> SystemStack & = delete;

  void push(std::unique_ptr<System> system);
  void start();
  void frame_begin(Timestep frame_time);
  void frame_end(Timestep frame_time);
  void shutdown();

private:
  std::vector<std::unique_ptr<System>> m_systems;
  std::size_t m_started_count{};
};

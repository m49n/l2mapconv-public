#include "SystemStack.h"

#include <utility>

SystemStack::~SystemStack() { shutdown(); }

void SystemStack::push(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void SystemStack::start() {
  while (m_started_count < m_systems.size()) {
    m_systems[m_started_count]->start();
    ++m_started_count;
  }
}

void SystemStack::frame_begin(Timestep frame_time) {
  for (const auto &system : m_systems) {
    system->frame_begin(frame_time);
  }
}

void SystemStack::frame_end(Timestep frame_time) {
  for (auto system = m_systems.rbegin(); system != m_systems.rend(); ++system) {
    (*system)->frame_end(frame_time);
  }
}

void SystemStack::shutdown() {
  while (m_systems.size() > m_started_count) {
    m_systems.pop_back();
  }
  while (m_started_count > 0) {
    m_systems.back()->stop();
    m_systems.pop_back();
    --m_started_count;
  }
}

#include "SystemStack.h"
#include "TestSupport.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class RecordingSystem final : public System {
public:
  RecordingSystem(std::string name, std::vector<std::string> &events)
      : m_name{std::move(name)}, m_events{events} {}

  ~RecordingSystem() override { m_events.push_back("destroy " + m_name); }

  void start() override { m_events.push_back("start " + m_name); }
  void stop() override { m_events.push_back("stop " + m_name); }

private:
  std::string m_name;
  std::vector<std::string> &m_events;
};

} // namespace

auto run_system_stack_tests() -> int {
  auto failures = 0;
  std::vector<std::string> events;

  {
    SystemStack systems;
    systems.push(std::make_unique<RecordingSystem>("camera", events));
    systems.push(std::make_unique<RecordingSystem>("streaming", events));
    systems.push(std::make_unique<RecordingSystem>("ui", events));
    systems.start();
    systems.shutdown();
  }

  failures += expect(
      events == std::vector<std::string>{"start camera", "start streaming",
                                         "start ui", "stop ui", "destroy ui",
                                         "stop streaming", "destroy streaming",
                                         "stop camera", "destroy camera"},
      "system stack stops and destroys systems in reverse ownership order");
  return failures;
}

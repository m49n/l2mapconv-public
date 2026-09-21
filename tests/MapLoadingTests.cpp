#include "MapLoadQueue.h"
#include "MapLoadingWorker.h"
#include "MapSource.h"
#include "TestSupport.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

auto region(MapCoordinate coordinate) -> MapRegion {
  return {coordinate,
          std::to_string(coordinate.x) + "_" + std::to_string(coordinate.y),
          {}};
}

auto request(MapCoordinate coordinate, MapLayer layer, std::uint64_t generation,
             MapLoadPriority priority) -> MapLoadRequest {
  return {{coordinate, layer}, region(coordinate), generation, priority};
}

struct BlockingSourceState {
  auto wait_for_calls(int expected) -> bool {
    std::unique_lock lock{mutex};
    return changed.wait_for(lock, std::chrono::seconds{5},
                            [this, expected] { return calls >= expected; });
  }

  void release_through(int call) {
    {
      std::lock_guard lock{mutex};
      released_through = call;
    }
    changed.notify_all();
  }

  std::mutex mutex;
  std::condition_variable changed;
  int calls{};
  int released_through{};
};

class BlockingMapSource final : public MapSource {
public:
  explicit BlockingMapSource(std::shared_ptr<BlockingSourceState> state)
      : m_state{std::move(state)} {}

  auto load(const MapRegion &map_region, MapLayer) -> Map override {
    std::unique_lock lock{m_state->mutex};
    const auto call = ++m_state->calls;
    m_state->changed.notify_all();
    m_state->changed.wait(
        lock, [this, call] { return m_state->released_through >= call; });
    lock.unlock();

    Map map;
    map.name = map_region.name;
    return map;
  }

private:
  std::shared_ptr<BlockingSourceState> m_state;
};

} // namespace

auto run_map_loading_tests() -> int {
  auto failures = 0;

  MapLoadQueue priority_queue;
  for (auto index = 0; index < 241; ++index) {
    priority_queue.enqueue(request({index, 0}, MapLayer::Terrain, 1,
                                   MapLoadPriority::ManualTerrain));
  }
  priority_queue.enqueue(
      request({22, 23}, MapLayer::Detail, 1, MapLoadPriority::NeighborDetail));
  const auto current_detail =
      request({22, 22}, MapLayer::Detail, 1, MapLoadPriority::CurrentDetail);
  priority_queue.enqueue(current_detail);
  const auto first = priority_queue.pop();
  failures += expect(first && first->key == current_detail.key,
                     "current detail outranks 241 manual terrain requests");

  MapLoadQueue coalesced;
  const auto old_request =
      request({22, 22}, MapLayer::Terrain, 1, MapLoadPriority::ManualTerrain);
  const auto new_request = request({22, 22}, MapLayer::Terrain, 2,
                                   MapLoadPriority::AutomaticTerrain);
  coalesced.enqueue(old_request);
  coalesced.enqueue(new_request);
  const auto newest = coalesced.pop();
  failures += expect(newest && newest->generation == 2 && coalesced.empty(),
                     "queue retains only the newest generation per key");
  failures += expect(!coalesced.is_current(old_request) &&
                         coalesced.is_current(new_request),
                     "popped requests retain current-generation identity");

  MapLoadQueue active_not_duplicated;
  active_not_duplicated.enqueue(old_request);
  const auto active = active_not_duplicated.pop();
  active_not_duplicated.enqueue(request({22, 22}, MapLayer::Terrain, 1,
                                        MapLoadPriority::AutomaticTerrain));
  failures +=
      expect(active && !active_not_duplicated.pop(),
             "reprioritizing an active generation does not duplicate it");

  MapLoadQueue cancelled;
  cancelled.enqueue(old_request);
  cancelled.cancel(old_request.key);
  failures +=
      expect(!cancelled.pop(), "cancelled coordinates are never popped");

  MapLoadQueue stress;
  for (std::uint64_t generation = 1; generation <= 100; ++generation) {
    const auto next = request({5, 6}, MapLayer::Terrain, generation,
                              MapLoadPriority::ManualTerrain);
    stress.enqueue(next);
    if (generation % 3 == 0) {
      stress.cancel(next.key);
      stress.enqueue(next);
    }
  }
  const auto latest = stress.pop();
  failures +=
      expect(latest && latest->generation == 100 && stress.empty(),
             "100 generation stress accepts exactly the latest request");

  {
    const auto state = std::make_shared<BlockingSourceState>();
    MapLoadingWorker worker{std::make_unique<BlockingMapSource>(state)};
    worker.reconcile({old_request}, {});
    const auto first_started = state->wait_for_calls(1);
    failures += expect(first_started, "worker starts the first request");
    if (!first_started) {
      state->release_through(100);
    } else {
      worker.reconcile({new_request}, {});
      state->release_through(1);
      const auto second_started = state->wait_for_calls(2);
      failures +=
          expect(second_started, "worker starts the superseding request");
      const auto completed = worker.take_completed();
      failures += expect(completed.size() == 1 &&
                             completed.front().request.generation == 1 &&
                             !worker.is_current(completed.front().request),
                         "completed stale result is reported but rejected");
      const auto started = worker.take_started();
      failures +=
          expect(started.size() == 2,
                 "worker reports loading transitions to the main thread");
      state->release_through(2);
    }
  }

  {
    const auto state = std::make_shared<BlockingSourceState>();
    auto worker = std::make_unique<MapLoadingWorker>(
        std::make_unique<BlockingMapSource>(state));
    worker->reconcile({old_request}, {});
    const auto started = state->wait_for_calls(1);
    failures += expect(started, "shutdown test reaches an active load");
    std::atomic_bool destroyed{false};
    std::thread destroyer{[owned = std::move(worker), &destroyed]() mutable {
      owned.reset();
      destroyed = true;
    }};
    state->release_through(100);
    destroyer.join();
    failures += expect(destroyed.load(),
                       "worker destructor joins after active load completes");
  }

  return failures;
}

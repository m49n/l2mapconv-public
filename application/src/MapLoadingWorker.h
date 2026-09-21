#pragma once

#include "MapLoadService.h"
#include "MapSource.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class MapLoadingWorker final : public MapLoadService {
public:
  explicit MapLoadingWorker(std::unique_ptr<MapSource> source);
  ~MapLoadingWorker() override;

  void reconcile(std::vector<MapLoadRequest> requests,
                 std::vector<MapLoadKey> cancellations) override;
  auto take_started() -> std::vector<MapLoadRequest> override;
  auto take_completed() -> std::vector<MapLoadResult> override;
  auto is_current(const MapLoadRequest &request) const -> bool override;

private:
  std::unique_ptr<MapSource> m_source;
  mutable std::mutex m_mutex;
  std::condition_variable m_changed;
  MapLoadQueue m_queue;
  std::vector<MapLoadRequest> m_started;
  std::vector<MapLoadResult> m_completed;
  bool m_stopping{};
  std::thread m_thread;

  void run();
};

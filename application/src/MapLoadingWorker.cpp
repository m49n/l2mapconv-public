#include "MapLoadingWorker.h"

#include <utility>

MapLoadingWorker::MapLoadingWorker(std::unique_ptr<MapSource> source)
    : m_source{std::move(source)}, m_thread{[this] { run(); }} {}

MapLoadingWorker::~MapLoadingWorker() {
  {
    std::lock_guard lock{m_mutex};
    m_stopping = true;
    m_queue.clear_pending();
  }
  m_changed.notify_all();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void MapLoadingWorker::reconcile(std::vector<MapLoadRequest> requests,
                                 std::vector<MapLoadKey> cancellations) {
  {
    std::lock_guard lock{m_mutex};
    if (m_stopping) {
      return;
    }
    for (const auto key : cancellations) {
      m_queue.cancel(key);
    }
    for (auto &request : requests) {
      m_queue.enqueue(std::move(request));
    }
  }
  m_changed.notify_one();
}

auto MapLoadingWorker::take_started() -> std::vector<MapLoadRequest> {
  std::lock_guard lock{m_mutex};
  std::vector<MapLoadRequest> result;
  result.swap(m_started);
  return result;
}

auto MapLoadingWorker::take_completed() -> std::vector<MapLoadResult> {
  std::lock_guard lock{m_mutex};
  std::vector<MapLoadResult> result;
  result.swap(m_completed);
  return result;
}

auto MapLoadingWorker::is_current(const MapLoadRequest &request) const -> bool {
  std::lock_guard lock{m_mutex};
  return m_queue.is_current(request);
}

void MapLoadingWorker::run() {
  while (true) {
    std::optional<MapLoadRequest> request;
    {
      std::unique_lock lock{m_mutex};
      m_changed.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
      if (m_stopping) {
        return;
      }
      request = m_queue.pop();
      if (!request) {
        continue;
      }
      m_started.push_back(*request);
    }

    MapLoadResult result{.request = *request, .payload = {}, .error = {}};
    try {
      const auto cancel = [this, &request] {
        std::lock_guard lock{m_mutex};
        return m_stopping || !m_queue.is_current(*request);
      };
      result.payload = m_source->load(request->region, request->key.layer,
                                      cancel);
    } catch (const std::exception &error) {
      result.error = error.what();
    }

    {
      std::lock_guard lock{m_mutex};
      if (!m_stopping && m_queue.is_current(*request)) {
        m_completed.push_back(std::move(result));
      }
    }
  }
}

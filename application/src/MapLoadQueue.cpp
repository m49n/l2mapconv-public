#include "MapLoadQueue.h"

#include <algorithm>
#include <tuple>
#include <utility>

void MapLoadQueue::enqueue(MapLoadRequest request) {
  const auto latest = m_latest.find(request.key);
  if (latest != m_latest.end()) {
    if (latest->second.generation > request.generation) {
      return;
    }
    if (latest->second.generation == request.generation) {
      const auto pending =
          std::find_if(m_pending.begin(), m_pending.end(),
                       [&request](const auto &candidate) {
                         return candidate.key == request.key &&
                                candidate.generation == request.generation;
                       });
      if (pending != m_pending.end()) {
        *pending = request;
        latest->second = std::move(request);
      }
      return;
    }
  }

  std::erase_if(m_pending, [&request](const MapLoadRequest &pending) {
    return pending.key == request.key;
  });
  m_latest.insert_or_assign(request.key, request);
  m_pending.push_back(std::move(request));
}

void MapLoadQueue::cancel(MapLoadKey key) {
  m_latest.erase(key);
  std::erase_if(m_pending, [key](const MapLoadRequest &pending) {
    return pending.key == key;
  });
}

void MapLoadQueue::clear_pending() { m_pending.clear(); }

auto MapLoadQueue::pop() -> std::optional<MapLoadRequest> {
  std::erase_if(m_pending, [this](const MapLoadRequest &request) {
    return !is_current(request);
  });
  if (m_pending.empty()) {
    return std::nullopt;
  }

  const auto less = [](const MapLoadRequest &left,
                       const MapLoadRequest &right) {
    return std::tie(left.priority, left.key.coordinate.x, left.key.coordinate.y,
                    left.key.layer, left.generation) <
           std::tie(right.priority, right.key.coordinate.x,
                    right.key.coordinate.y, right.key.layer, right.generation);
  };
  const auto selected =
      std::min_element(m_pending.begin(), m_pending.end(), less);
  auto request = std::move(*selected);
  m_pending.erase(selected);
  return request;
}

auto MapLoadQueue::is_current(const MapLoadRequest &request) const -> bool {
  const auto latest = m_latest.find(request.key);
  return latest != m_latest.end() &&
         latest->second.generation == request.generation;
}

auto MapLoadQueue::empty() const -> bool { return m_pending.empty(); }

auto MapLoadQueue::size() const -> std::size_t { return m_pending.size(); }

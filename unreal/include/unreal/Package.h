#pragma once

#include "Archive.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace unreal {

class Package {
public:
  explicit Package(Archive &archive) : m_archive{archive} {}

  template <typename T>
  auto load_object(const std::string &name) const -> std::shared_ptr<T> {
    return std::dynamic_pointer_cast<T>(
        m_archive.object_loader.load_object(name));
  }

  template <typename T>
  void load_objects(const std::string &class_name,
                    std::vector<std::shared_ptr<T>> &objects) const {

    m_archive.object_loader.load_objects(class_name, objects);
  }

  auto name() const -> std::string { return std::string{m_archive.name}; }

  friend auto operator<<(std::ostream &output, const Package &package)
      -> std::ostream &;

private:
  Archive &m_archive;
};

} // namespace unreal

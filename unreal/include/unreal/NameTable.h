#pragma once

#include "Name.h"

#include <utils/NonCopyable.h>

#include <string>
#include <unordered_set>
#include <utility>

namespace unreal {

class NameTable : public utils::NonCopyable {
public:
  NameTable() {}
  NameTable(NameTable &&other) : m_names{std::move(other.m_names)} {}

  auto name(const std::string &string) -> Name {
    const auto name = m_names.insert(string).first;
    return Name{name->data(), name->length()};
  }

  auto none_name() -> Name { return name(Name::NONE); }

private:
  std::unordered_set<std::string> m_names;
};

} // namespace unreal

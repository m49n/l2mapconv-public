#pragma once
#include "Json.h"
#include <string>
#include <string_view>
#include <vector>
namespace territory {
enum class IssueKind {
  MissingPackage,
  MissingObject,
  Unsupported,
  Corrupt,
  Simplified,
  WaterUnresolved
};
struct Issue {
  IssueKind kind;
  std::string source, reason;
  std::vector<std::string> surfaces;
};
struct Report {
  std::vector<Issue> issues;
  Json maps = Json::array();
};
Json to_json(const Report &, std::string_view job_id);
} // namespace territory

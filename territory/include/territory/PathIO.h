#pragma once
#include "Json.h"
#include <filesystem>
#include <string>
#include <string_view>
namespace territory {
std::string path_utf8(const std::filesystem::path &);
std::filesystem::path path_from_utf8(std::string_view);
bool is_within(const std::filesystem::path &child,
               const std::filesystem::path &parent);
std::filesystem::path create_job_directory(const std::filesystem::path &output,
                                           const std::filesystem::path &client);
void write_json_atomic(const std::filesystem::path &, const Json &,
                       bool replace_owned_status);
Json read_json(const std::filesystem::path &);
void request_cancel(const std::filesystem::path &job_directory);
bool cancellation_requested(const std::filesystem::path &job_directory);
} // namespace territory

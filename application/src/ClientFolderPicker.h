#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

auto choose_client_root_folder() -> std::optional<std::filesystem::path>;
void show_client_selection_error(std::string_view message);

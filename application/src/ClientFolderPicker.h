#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

auto choose_client_root_folder() -> std::optional<std::filesystem::path>;
auto choose_directory(std::wstring_view title) -> std::optional<std::filesystem::path>;
void show_client_selection_error(std::string_view message);
auto choose_file(std::wstring_view title, bool save_new = false,
                 std::wstring_view default_extension = L"json")
    -> std::optional<std::filesystem::path>;

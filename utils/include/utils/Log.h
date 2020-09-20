#pragma once

#include <ostream>
#include <string>
#include <unordered_set>

namespace utils {

enum LogLevel {
  LOG_NONE,

  LOG_FATAL,
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG,

  LOG_ALL,
};

class Log {
public:
  using endl_type = decltype(std::endl<char, std::char_traits<char>>);

  inline static LogLevel level = LOG_INFO;
  inline static std::unordered_set<std::string> spaces;
  inline static bool colored = true;

  Log(LogLevel log_level, const std::string &space = "");

  auto operator<<(endl_type endl) -> Log &;

  template <typename T> auto operator<<(const T &value) -> Log & {
    if (check_filter()) {
      m_output << value;
    }

    return *this;
  }

private:
  enum class Color : int {
    Clear = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97,
  };

  LogLevel m_log_level;
  std::string m_space;
  std::ostream &m_output;

  auto log_level_prefix() const -> std::string;
  auto check_filter() const -> bool;
  auto ansi_color(Color color) const -> std::string;
};

} // namespace utils

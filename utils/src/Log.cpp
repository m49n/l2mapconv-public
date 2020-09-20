#include <utils/Log.h>

#include <iostream>

namespace utils {

Log::Log(LogLevel log_level, const std::string &space)
    : m_log_level{log_level}, m_space{space}, m_output{std::cout} {

  if (check_filter()) {
    if (m_space.empty()) {
      m_output << log_level_prefix() << ": ";
    } else {
      m_output << log_level_prefix() << " (" << m_space << "): ";
    }
  }
}

auto Log::operator<<(endl_type endl) -> Log & {
  if (check_filter()) {
    m_output << ansi_color(Color::Clear) << endl;
  }

  return *this;
}

auto Log::log_level_prefix() const -> std::string {
  switch (m_log_level) {
  case LOG_FATAL: {
    return ansi_color(Color::Red) + "[FATAL]";
  }
  case LOG_ERROR: {
    return ansi_color(Color::BrightRed) + "[ERROR]";
  }
  case LOG_WARN: {
    return ansi_color(Color::Yellow) + "[WARN]";
  }
  case LOG_INFO: {
    return ansi_color(Color::Clear) + "[INFO]";
  }
  case LOG_DEBUG: {
    return ansi_color(Color::White) + "[DEBUG]";
  }
  default: {
    return ansi_color(Color::BrightWhite) + "[UNKNOWN]";
  }
  }
}

auto Log::check_filter() const -> bool {
  return m_log_level <= level &&
         (m_space.empty() || spaces.empty() || spaces.count(m_space) != 0);
}

auto Log::ansi_color(Color color) const -> std::string {
  if (!colored) {
    return "";
  }

  return "\033[" + std::to_string(static_cast<int>(color)) + "m";
}

} // namespace utils

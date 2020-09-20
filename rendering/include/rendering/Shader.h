#pragma once

#include "Context.h"

#include <utils/NonCopyable.h>

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace rendering {

class Shader : public utils::NonCopyable {
public:
  explicit Shader(Context &context, const std::string &vertex,
                  const std::string &fragment);

  virtual ~Shader();

  Shader(Shader &&other) noexcept;
  auto operator=(Shader &&other) noexcept -> Shader &;

  void bind() const;
  void unbind() const;

  void load(const std::string &name, int value) const;
  void load(const std::string &name, float value) const;
  void load(const std::string &name, const glm::vec2 &value) const;
  void load(const std::string &name, const glm::vec3 &value) const;
  void load(const std::string &name, const glm::vec4 &value) const;
  void load(const std::string &name, const glm::mat4 &value) const;

private:
  Context &m_context;
  unsigned int m_program;
  mutable std::unordered_map<std::string, unsigned int> m_uniform_locations;

  auto compile(const std::string &source, unsigned int type) const
      -> unsigned int;
  auto uniform_location(const std::string &name) const -> int;
  void check_for_errors(unsigned int handle) const;
};

} // namespace rendering

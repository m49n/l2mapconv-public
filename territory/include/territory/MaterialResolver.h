#pragma once
#include "Diagnostics.h"
#include "MaterialGraph.h"
#include <functional>
#include <memory>
namespace territory {
class MaterialResolver {
public:
  MaterialResolver(MaterialLibrary &, Report &);
  ~MaterialResolver();
  MaterialResolver(const MaterialResolver &) = delete;
  std::uint32_t resolve(const std::shared_ptr<unreal::Material> &,
                        const unreal::AssetReference &,
                        std::string_view surface);
  std::uint32_t resolve_object(const std::shared_ptr<unreal::Object> &,
                               const unreal::AssetReference &,
                               std::string_view surface);
  void set_package_probe(std::function<bool(std::string_view)> probe);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
} // namespace territory

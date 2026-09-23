#pragma once

#include "Index.h"
#include "ObjectLoader.h"
#include "Property.h"

#include <utils/Assert.h>
#include <utils/Log.h>

#include <memory>
#include <stdexcept>

namespace unreal {

enum class ObjectRefRequirement {
  Required,
  Optional,
};

template <typename T,
          ObjectRefRequirement requirement = ObjectRefRequirement::Required>
class ObjectRef {
public:
  ObjectRef() : m_index{}, m_object_loader{nullptr}, m_object{nullptr} {}

  auto operator->() const -> std::shared_ptr<T> { return load_object<T>(); }
  operator std::shared_ptr<T>() const { return load_object<T>(); }
  operator T &() const {
    auto object = load_object<T>();
    if (!object) throw std::runtime_error("Unresolved required Unreal object");
    return *object;
  }

  template <typename U> auto as() const -> std::shared_ptr<U> {
    return load_object<U>();
  }
  auto reference() const -> AssetReference {
    return m_object_loader && m_index != 0 ? m_object_loader->reference(m_index) : AssetReference{};
  }
  auto has_reference() const -> bool { return m_index != 0; }
  auto untyped() const -> std::shared_ptr<Object> {
    if (m_index == 0) {
      if constexpr (requirement == ObjectRefRequirement::Optional) return nullptr;
      throw std::runtime_error("Null required Unreal object reference");
    }
    if (!m_object_loader) throw std::runtime_error("Uninitialized Unreal object loader");
    if (!m_load_attempted) {
      m_object = m_object_loader->load_object(m_index);
      m_load_attempted = true;
    }
    return m_object;
  }

  void from_property(const Property &property, Archive &archive) {
    ASSERT(m_object_loader == nullptr, "Unreal",
           "Object Loader must be unititalized");

    m_index.value = property.index_value.value;
    m_object_loader = &archive.object_loader;
  }

  friend auto operator>>(Archive &archive, ObjectRef &object_ref) -> Archive & {
    ASSERT(object_ref.m_object_loader == nullptr, "Unreal",
           "Object Loader must be unititalized");

    archive >> object_ref.m_index;
    object_ref.m_object_loader = &archive.object_loader;
    return archive;
  }

  operator bool() const { return load_object<T>() != nullptr; }

private:
  Index m_index;
  const ObjectLoader *m_object_loader;

  mutable std::shared_ptr<Object> m_object;
  mutable bool m_load_attempted{false};

  template <typename U> auto load_object() const -> std::shared_ptr<U> {
    return std::dynamic_pointer_cast<U>(untyped());
  }
};

} // namespace unreal

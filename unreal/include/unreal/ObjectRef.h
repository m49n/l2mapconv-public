#pragma once

#include "Index.h"
#include "ObjectLoader.h"
#include "Property.h"

#include <utils/Assert.h>
#include <utils/Log.h>

#include <memory>

namespace unreal {

enum class ObjectRefConstraint {
  Required,
  Optional,
};

template <typename T,
          ObjectRefConstraint constraint = ObjectRefConstraint::Required>
class ObjectRef {
public:
  ObjectRef() : m_index{}, m_object_loader{nullptr}, m_object{nullptr} {}

  auto operator->() const -> std::shared_ptr<T> { return load_object<T>(); }
  operator std::shared_ptr<T>() const { return load_object<T>(); }
  operator T &() const { return *load_object<T>(); }

  template <typename U> auto as() const -> std::shared_ptr<U> {
    return load_object<U>();
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

  mutable std::shared_ptr<T> m_object;

  template <typename U> auto load_object() const -> std::shared_ptr<U> {
    if (constraint == ObjectRefConstraint::Optional && m_index == 0) {
      return nullptr;
    }

    if (m_object == nullptr) {
      ASSERT(m_object_loader != nullptr, "Unreal",
             "Object loader must be initialized");
      ASSERT(m_index != 0, "Unreal", "Index can't be equal to zero");

      m_object =
          std::dynamic_pointer_cast<U>(m_object_loader->load_object(m_index));
    }

    return std::dynamic_pointer_cast<U>(m_object);
  }
};

} // namespace unreal

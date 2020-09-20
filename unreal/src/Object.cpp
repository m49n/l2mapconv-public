#include "pch.h"

#include <unreal/Object.h>

namespace unreal {

void Object::deserialize() {
  if ((flags & RF_HasStack) != 0) {
    archive >> state_frame;
  }

  if ((flags & RF_Native) == 0) {
    const auto properties = archive.property_extractor.extract_properties();

    for (const auto &property : properties) {
      if (!set_property(property)) {
        utils::Log(utils::LOG_DEBUG, "Unreal")
            << "Unconsumed property: " << property.name << std::endl
            << std::endl;
      }
    }
  }
}

auto Object::full_name() const -> std::string {
  return std::string{archive.name} + "." + std::string{name};
}

auto operator>>(Archive &archive, StateFrame &state_frame) -> Archive & {
  archive >> state_frame.node >> state_frame.state_node >>
      state_frame.probe_mask >> state_frame.latent_action;

  if (state_frame.node != 0) {
    archive >> state_frame.offset;
  }

  return archive;
}

} // namespace unreal

#pragma once
#include "MapSelectionContext.h"

auto map_focus_position(glm::vec2 center, std::optional<geometry::Box> bounds,
                        float current_z) -> glm::vec3;
auto map_focus_target(const MapSelectionContext &selection,
                      MapCoordinate clicked, float current_z)
    -> std::optional<glm::vec3>;

#include "pch.h"

#include <rendering/EntityRenderer.h>

namespace rendering {

EntityRenderer::EntityRenderer(Context &context, const Camera &camera)
    : m_context{context}, m_camera{camera} {}

void EntityRenderer::render(const Scene &scene, const FrameSettings &settings,
                            int &draws) const {

  //  utils::Timer timer{__func__};

  const auto projection_matrix = m_camera.projection_matrix();
  const auto view_matrix = m_camera.view_matrix();
  const auto frustum = m_camera.frustum();

  // Surface type branch
  for (const auto &type_branch : scene.tree()) {
    const auto surface_type = type_branch.first;

    if ((settings.surface_filter & surface_type) != surface_type) {
      continue;
    }

    // Shader branch
    for (const auto &shader_branch : type_branch.second) {
      const auto &shader = shader_branch.first;

      shader->bind();
      shader->load_camera(m_camera.position());
      shader->load_projection_matrix(projection_matrix);
      shader->load_view_matrix(view_matrix);

      // Texture branch
      for (const auto &texture_branch : shader_branch.second) {
        const auto &texture = texture_branch.first;

        if (texture == nullptr ||
            (settings.surface_textures & surface_type) == 0) {

          unbind_current_texture();
        } else {
          texture->bind(0);
          shader->load_texture_unit(0);
          shader->load_color({});
        }

        // Mesh branch
        for (const auto &mesh_branch : texture_branch.second) {
          const auto &mesh = mesh_branch.first;

          // Surfaces
          for (const auto &entry : mesh_branch.second) {
            if (!scene.group_visible(entry.group)) {
              continue;
            }
            const auto *entity = entry.entity;
            const auto *surface = entry.surface;
            const auto &bounding_box = entity->bounding_box();

            // Frustum culling
            if (settings.culling && !bounding_box.is_zero() &&
                !frustum.intersects(bounding_box)) {

              continue;
            }

            if (entity->wireframe()) {
              GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
            }

            if (texture == nullptr ||
                (settings.surface_textures & surface_type) == 0) {

              shader->load_color(surface->material.color);
            }

            shader->load_model_matrix(entity->model_matrix());
            mesh->draw(*surface);
            draws++;

            if (!settings.wireframe && entity->wireframe()) {
              GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
            }
          }
        }
      }
    }
  }
}

void EntityRenderer::unbind_current_texture() const {
  if (m_context.texture.texture != 0) {
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    m_context.texture.texture = 0;
  }
}

} // namespace rendering

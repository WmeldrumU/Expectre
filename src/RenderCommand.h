#ifndef RENDER_COMMAND_H
#define RENDER_COMMAND_H
#include <variant>
#include <vector>

#include "Image.h"
#include "Mesh.h"
#include <glm/glm.hpp>

namespace Expectre {

struct UploadTextureCmd {
  Image image;
  ImagePixelData pixel_data;
};

struct UploadMeshCmd {
  MeshHandle handle;
  // vertex/index data of prim
  PendingPrimitiveUpload pending_prim_upload;

  // Explicitly disable copying.
  UploadMeshCmd(const UploadMeshCmd &) = delete;
  UploadMeshCmd &operator=(const UploadMeshCmd &) = delete;

  // Explicitly enable moving.
  UploadMeshCmd(UploadMeshCmd &&) = default;
  UploadMeshCmd &operator=(UploadMeshCmd &&) = default;
};

struct DrawMeshCmd {
  glm::mat4 world_matrix;
  uint32_t mesh_id;
  uint32_t index_count;
  uint32_t first_index;
  uint32_t vertex_offset;

  // DrawMeshCmd() noexcept = default;
  // Explicitly disable copying.
  DrawMeshCmd(const DrawMeshCmd &) = delete;
  DrawMeshCmd &operator=(const DrawMeshCmd &) = delete;

  // Explicitly enable moving.
  DrawMeshCmd(DrawMeshCmd &&) noexcept = default;
  DrawMeshCmd &operator=(DrawMeshCmd &&) noexcept = default;
};

struct RenderCommands {
  std::vector<UploadTextureCmd> upload_tex_cmds;
  std::vector<UploadMeshCmd> upload_mesh_cmds;
  std::vector<DrawMeshCmd> draw_cmds;
  void clear() {
    upload_tex_cmds.clear();
    upload_mesh_cmds.clear();
    draw_cmds.clear();
  }

  bool empty() {
    return upload_tex_cmds.empty() && upload_mesh_cmds.empty() &&
           draw_cmds.empty();
  }
};

using RenderCommand = std::variant<UploadMeshCmd, DrawMeshCmd>;

} // namespace Expectre
#endif // RENDER_COMMAND_H
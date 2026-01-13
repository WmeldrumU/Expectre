#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/defs.h>

namespace Expectre {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec3 normal;
  glm::vec2 tex_coord;

};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::string name; // Name of the mesh
                    // std::vector<Texture> textures; // Textures associated
                    // with the mesh
};

static Mesh import_mesh(aiMesh *ai_mesh) {

  Mesh mesh{};
  mesh.name = ai_mesh->mName.C_Str();
  mesh.vertices.reserve(ai_mesh->mNumVertices);
  mesh.indices.reserve(ai_mesh->mNumFaces * 3);

  // Iterate through each vertex of the mesh
  for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++) {
    Expectre::Vertex vertex;

    // Positions
    vertex.pos.x = ai_mesh->mVertices[j].x;
    vertex.pos.y = ai_mesh->mVertices[j].y;
    vertex.pos.z = ai_mesh->mVertices[j].z;

    // Normals
    if (ai_mesh->HasNormals()) {
      vertex.normal.x = ai_mesh->mNormals[j].x;
      vertex.normal.y = ai_mesh->mNormals[j].y;
      vertex.normal.z = ai_mesh->mNormals[j].z;
    }

    // Texture Coordinates
    if (ai_mesh->mTextureCoords[0]) { // Check if the mesh contains texture
                                      // coordinates
      vertex.tex_coord = {ai_mesh->mTextureCoords[0][j].x,
                          ai_mesh->mTextureCoords[0][j].y};
    } else {
      vertex.tex_coord = glm::vec2(0.0f, 0.0f);
    }

    // set vertex color to black for now
    vertex.color = glm::vec3(0.0f);

    mesh.vertices.push_back(vertex);
  }

  // Indices
  for (auto j = 0; j < ai_mesh->mNumFaces; j++) {
    aiFace &face = ai_mesh->mFaces[j];
    for (auto k = 0; k < face.mNumIndices; k++) {
      mesh.indices.push_back(face.mIndices[k]);
    }
  }
  return mesh;
}

} // namespace Expectre
#endif // MESH_H
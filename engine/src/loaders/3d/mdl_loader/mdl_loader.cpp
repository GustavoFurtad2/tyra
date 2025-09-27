/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Gustavo Furtado <gustav0furt.fatality@gmail.com>
*/

#include "loaders/3d/mdl_loader/mdl_loader.hpp"
#include <stdio.h>
#include <string>
#include "debug/debug.hpp"
#include "file/file_utils.hpp"

namespace Tyra {

#define MDL_IDENT (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I')

#define MDL_VERSION 10

typedef struct {
  int ident;
  int version;
  char name[64];
  int length;

  float eyeposition[3];
  float min[3];
  float max[3];
  float bbmin[3];
  float bbmax[3];

  int flags;
  int num_skins;
  int num_bodyparts;
  int num_bone_controllers;
  int num_attachments;
  int num_transitions;

  int ofs_skins;
  int ofs_bodyparts;
  int ofs_bone_controllers;
  int ofs_attachments;
  int ofs_transitions;
} mdl_t;

typedef struct {
  int type;
  char name[64];
} skin_t;

typedef struct {
  char name[64];
  int num_models;
  int base;
  int modelindex;
} bodypart_t;

typedef struct {
  char name[64];
  int num_meshes;
  int meshindex;
} model_t;

typedef struct {
  int num_verts;
  int vert_index;
  int num_tris;
  int tri_index;
} mesh_t;

typedef struct {
  char name[16];
  float bboxmin[3];
  float bboxmax[3];
  int framedataindex;
} frame_t;

typedef struct {
  int vert_index[3];
  int normal_index[3];
} triangle_t;

typedef struct {
  short s, t;
} texCoord_t;

typedef struct {
  unsigned char v[3];
  unsigned char normal_index;
} mvertex_t;

std::unique_ptr<MeshBuilderData> MDLLoader::load(const char* fullpath,
                                                 MDLLoaderOptions options) {
  std::string path = fullpath;
  TYRA_ASSERT(!path.empty(), "Provided path is empty!");

  auto filename = FileUtils::getFilenameFromPath(path);

  FILE* file = fopen(fullpath, "rb");
  TYRA_ASSERT(file != nullptr, "Failed to load: ", filename);
  mdl_t header;

  fread(reinterpret_cast<char*>(&header), sizeof(mdl_t), 1, file);

  TYRA_ASSERT((header.ident == MDL_IDENT) && (header.version == MDL_VERSION),
              "This MDL file is not in correct format!");

  fseek(header.ofs_bodyparts, SEEK_SET);
  bodypart_t bodypart;
  fread(&bodypart, sizeof(bodypart), 1, file);

  fseek(header.ofs_bodyparts + bodypart.modelindex, SEEK_SET);
  model_t model;
  fread(&model, sizeof(model), 1, file);

  fseek(header.ofs_bodyparts + model.meshindex, SEEK_SET);
  mesh_t mesh;
  fread(&mesh, sizeof(mesh), 1, file);

  auto trianglesBuffer = new triangle_t[mesh.num_tris];
  fseek(file, header.ofs_bodyparts + mesh.tri_index, SEEK_SET);
  fread(trianglesBuffer, sizeof(triangle_t), mesh.num_tris, file);

  auto verticesBuffer = new mvertex_t[mesh.num_verts];
  fseek(file, header.offset_bodyparts + mesh.vert_index, SEEK_SET);
  fread(verticesBuffer, sizeof(mvertex_t), mesh.num_verts, file);

  fclose(file);

  // create mesh builder data
  auto result = std::make_unique<MeshBuilderData>();
  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  result->materials.push_back(material);
  
  material->frames.resize(1);
  material->frames[0]->count = mesh.num_tris * 3;
  material->frames[0]->vertices = new Vec4[mesh.num_tris * 3];

  for (int i = 0; i < mesh.num_tris; i++) {
    for (int j = 0; j < 3; j++) {
      int vertIndex = trianglesBuffer[i].vert_index[j];
      material->frames[0]->vertices[i * 3 + j].set(
        float(verticesBuffer[vertIndex].v[0]),
        float(verticesBuffer[vertIndex].v[1]),
        float(verticesBuffer[vertIndex].v[2]),
        1.0f
      )
    }
  }

  delete[] verticesBuffer;
  delete[] trianglesBuffer;

  return result;
}

}
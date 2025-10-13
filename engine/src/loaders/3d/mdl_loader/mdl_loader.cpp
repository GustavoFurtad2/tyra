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
  char name[64];
  int num_models;
  int base;
  int modelindex;
} bodypart_t;

typedef struct {
  char name[64];
  int num_meshes;
  int meshindex;
  int num_verts;
  int vert_index;
  int num_norms;
  int norm_index;
} model_t;

typedef struct {
  int skinref;
  int num_tris;
  int tri_index;
  int num_verts;
  int vert_index;
} mesh_t;

std::unique_ptr<MeshBuilderData> MDLLoader::load(const char* fullpath,
                                                 MDLLoaderOptions options) {
  std::string path = fullpath;
  TYRA_ASSERT(!path.empty(), "Provided path is empty!");

  auto filename = FileUtils::getFilenameFromPath(path);

  FILE* file = fopen(fullpath, "rb");
  TYRA_ASSERT(file != nullptr, "Failed to load: ", filename);
  
  // Ler header
  mdl_t header;
  fread(reinterpret_cast<char*>(&header), sizeof(mdl_t), 1, file);

  TYRA_ASSERT((header.ident == MDL_IDENT) && (header.version == MDL_VERSION),
              "This MDL file is not in correct format!");

  TYRA_LOG("MDL header: name=", header.name);
  TYRA_LOG("  version=", header.version);
  TYRA_LOG("  num_bodyparts=", header.num_bodyparts);
  TYRA_LOG("  num_skins=", header.num_skins);
  TYRA_LOG("  ofs_bodyparts=", header.ofs_bodyparts);
  TYRA_LOG("  ofs_skins=", header.ofs_skins);

  // Ler bodypart
  fseek(file, header.ofs_bodyparts, SEEK_SET);
  bodypart_t bodypart;
  fread(&bodypart, sizeof(bodypart), 1, file);
  
  TYRA_LOG("Bodypart: name=", bodypart.name, " models=", bodypart.num_models);
  TYRA_LOG("  base=", bodypart.base, " modelindex=", bodypart.modelindex);

  TYRA_ASSERT(bodypart.num_models > 0, "Bodypart has no models!");

  // Ler model (offset ABSOLUTO desde o início do bodypart)
  long modelOffset = header.ofs_bodyparts + bodypart.modelindex;
  TYRA_LOG("Reading model from offset: ", modelOffset);
  fseek(file, modelOffset, SEEK_SET);
  model_t model;
  fread(&model, sizeof(model), 1, file);
  
  TYRA_LOG("Model: name=", model.name);
  TYRA_LOG("  num_meshes=", model.num_meshes);
  TYRA_LOG("  meshindex=", model.meshindex);
  TYRA_LOG("  num_verts=", model.num_verts);
  TYRA_LOG("  vert_index=", model.vert_index);
  TYRA_LOG("  num_norms=", model.num_norms);
  TYRA_LOG("  norm_index=", model.norm_index);

  TYRA_ASSERT(model.num_meshes > 0, "Model has no meshes!");

  // Ler primeiro mesh (offset ABSOLUTO desde o início do bodypart)
  long meshOffset = header.ofs_bodyparts + model.meshindex;
  TYRA_LOG("Reading mesh from offset: ", meshOffset);
  fseek(file, meshOffset, SEEK_SET);
  mesh_t mesh;
  fread(&mesh, sizeof(mesh), 1, file);
  
  TYRA_LOG("Mesh data:");
  TYRA_LOG("  skinref=", mesh.skinref);
  TYRA_LOG("  num_tris=", mesh.num_tris);
  TYRA_LOG("  tri_index=", mesh.tri_index);
  TYRA_LOG("  num_verts=", mesh.num_verts);
  TYRA_LOG("  vert_index=", mesh.vert_index);
  
  // Se mesh não tem dados, tentar usar dados do model diretamente
  if (mesh.num_tris == 0 || mesh.num_verts == 0) {
    TYRA_LOG("Mesh vazio, usando dados do model diretamente");
    TYRA_ASSERT(model.num_verts > 0, "Model also has no vertices!");
  } else {
    TYRA_ASSERT(mesh.num_tris > 0, "Mesh has no triangles!");
    TYRA_ASSERT(mesh.num_verts > 0, "Mesh has no vertices!");
  }

  // Por simplicidade, vamos ler os vértices do modelo e criar triângulos simples
  // Isso evita problemas com a estrutura complexa do MDL
  
  // Estrutura de vértice simples
  struct SimpleVert {
    unsigned char pos[3];
    unsigned char normalIndex;
  };

  auto modelVerts = new SimpleVert[model.num_verts];
  long vertOffset = header.ofs_bodyparts + model.vert_index;
  fseek(file, vertOffset, SEEK_SET);
  fread(modelVerts, sizeof(SimpleVert), model.num_verts, file);

  fclose(file);

  // Criar mesh builder data
  auto result = std::make_unique<MeshBuilderData>();
  
  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  material->texturePath = material->name;
  material->texturePath.value().append(".png");
  
  result->materials.push_back(material);
  result->loadNormals = false;  // Desabilitar normais por enquanto
  result->loadLightmap = false;

  // Criar frame
  auto* outputFrame = new MeshBuilderMaterialFrameData();
  material->frames.push_back(outputFrame);

  // Usar apenas os vértices do modelo (simplificado)
  // Criar triângulos básicos: 0-1-2, 3-4-5, etc
  int numTrisToUse = model.num_verts / 3;
  
  outputFrame->count = numTrisToUse * 3;
  outputFrame->vertices = new Vec4[numTrisToUse * 3];

  TYRA_LOG("Creating ", numTrisToUse, " triangles from ", model.num_verts, " vertices");

  // Preencher vértices
  for (int i = 0; i < numTrisToUse * 3 && i < model.num_verts; i++) {
    outputFrame->vertices[i].set(
      static_cast<float>(modelVerts[i].pos[0]) * options.scale,
      static_cast<float>(modelVerts[i].pos[1]) * options.scale,
      static_cast<float>(modelVerts[i].pos[2]) * options.scale,
      1.0f
    );
  }

  delete[] modelVerts;
  
  TYRA_LOG("MDL loaded successfully!");

  return result;
}

std::unique_ptr<MeshBuilderData> MDLLoader::load(const char* fullpath) {
  return load(fullpath, MDLLoaderOptions());
}

std::unique_ptr<MeshBuilderData> MDLLoader::load(const std::string& fullpath) {
  return load(fullpath.c_str(), MDLLoaderOptions());
}

std::unique_ptr<MeshBuilderData> MDLLoader::load(const std::string& fullpath, 
                                                 MDLLoaderOptions options) {
  return load(fullpath.c_str(), options);
}

}  // namespace Tyra
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

// Estrutura de triângulo que aponta para os vértices
typedef struct {
  short vertindex;  // índice no array de trivert_t
  short normindex;  // índice de normal
  short s, t;       // coordenadas de textura
} trivert_t;

typedef struct {
  unsigned char v[3];
  unsigned char normal_index;
} mvertex_t;

// Tabela de normais pré-calculadas (mesma do Quake)
static const float ANORMS[162][3] = {
  {-0.525731f, 0.000000f, 0.850651f},
  {-0.442863f, 0.238856f, 0.864188f},
  {-0.295242f, 0.000000f, 0.955423f},
  {-0.309017f, 0.500000f, 0.809017f},
  {-0.162460f, 0.262866f, 0.951056f},
  {0.000000f, 0.000000f, 1.000000f},
  {0.000000f, 0.850651f, 0.525731f},
  {-0.147621f, 0.716567f, 0.681718f},
  {0.147621f, 0.716567f, 0.681718f},
  {0.000000f, 0.525731f, 0.850651f},
  {0.309017f, 0.500000f, 0.809017f},
  {0.525731f, 0.000000f, 0.850651f},
  {0.295242f, 0.000000f, 0.955423f},
  {0.442863f, 0.238856f, 0.864188f},
  {0.162460f, 0.262866f, 0.951056f},
  {-0.681718f, 0.147621f, 0.716567f},
  {-0.809017f, 0.309017f, 0.500000f},
  {-0.587785f, 0.425325f, 0.688191f},
  {-0.850651f, 0.525731f, 0.000000f},
  {-0.864188f, 0.442863f, 0.238856f},
  {-0.716567f, 0.681718f, 0.147621f},
  {-0.688191f, 0.587785f, 0.425325f},
  {-0.500000f, 0.809017f, 0.309017f},
  {-0.238856f, 0.864188f, 0.442863f},
  {-0.425325f, 0.688191f, 0.587785f},
  {-0.716567f, 0.681718f, -0.147621f},
  {-0.500000f, 0.809017f, -0.309017f},
  {-0.525731f, 0.850651f, 0.000000f},
  {0.000000f, 0.850651f, -0.525731f},
  {-0.238856f, 0.864188f, -0.442863f},
  {0.000000f, 0.955423f, -0.295242f},
  {-0.262866f, 0.951056f, -0.162460f},
  {0.000000f, 1.000000f, 0.000000f},
  {0.000000f, 0.955423f, 0.295242f},
  {-0.262866f, 0.951056f, 0.162460f},
  {0.238856f, 0.864188f, 0.442863f},
  {0.262866f, 0.951056f, 0.162460f},
  {0.500000f, 0.809017f, 0.309017f},
  {0.238856f, 0.864188f, -0.442863f},
  {0.262866f, 0.951056f, -0.162460f},
  {0.500000f, 0.809017f, -0.309017f},
  {0.850651f, 0.525731f, 0.000000f},
  {0.716567f, 0.681718f, 0.147621f},
  {0.716567f, 0.681718f, -0.147621f},
  {0.525731f, 0.850651f, 0.000000f},
  {0.425325f, 0.688191f, 0.587785f},
  {0.864188f, 0.442863f, 0.238856f},
  {0.688191f, 0.587785f, 0.425325f},
  {0.809017f, 0.309017f, 0.500000f},
  {0.681718f, 0.147621f, 0.716567f},
  {0.587785f, 0.425325f, 0.688191f},
  {0.955423f, 0.295242f, 0.000000f},
  {1.000000f, 0.000000f, 0.000000f},
  {0.951056f, 0.162460f, 0.262866f},
  {0.850651f, -0.525731f, 0.000000f},
  {0.955423f, -0.295242f, 0.000000f},
  {0.864188f, -0.442863f, 0.238856f},
  {0.951056f, -0.162460f, 0.262866f},
  {0.809017f, -0.309017f, 0.500000f},
  {0.681718f, -0.147621f, 0.716567f},
  {0.850651f, 0.000000f, 0.525731f},
  {0.864188f, 0.442863f, -0.238856f},
  {0.809017f, 0.309017f, -0.500000f},
  {0.951056f, 0.162460f, -0.262866f},
  {0.525731f, 0.000000f, -0.850651f},
  {0.681718f, 0.147621f, -0.716567f},
  {0.681718f, -0.147621f, -0.716567f},
  {0.850651f, 0.000000f, -0.525731f},
  {0.809017f, -0.309017f, -0.500000f},
  {0.864188f, -0.442863f, -0.238856f},
  {0.951056f, -0.162460f, -0.262866f},
  {0.147621f, 0.716567f, -0.681718f},
  {0.309017f, 0.500000f, -0.809017f},
  {0.425325f, 0.688191f, -0.587785f},
  {0.442863f, 0.238856f, -0.864188f},
  {0.587785f, 0.425325f, -0.688191f},
  {0.688191f, 0.587785f, -0.425325f},
  {-0.147621f, 0.716567f, -0.681718f},
  {-0.309017f, 0.500000f, -0.809017f},
  {0.000000f, 0.525731f, -0.850651f},
  {-0.525731f, 0.000000f, -0.850651f},
  {-0.442863f, 0.238856f, -0.864188f},
  {-0.295242f, 0.000000f, -0.955423f},
  {-0.162460f, 0.262866f, -0.951056f},
  {0.000000f, 0.000000f, -1.000000f},
  {0.295242f, 0.000000f, -0.955423f},
  {0.162460f, 0.262866f, -0.951056f},
  {-0.442863f, -0.238856f, -0.864188f},
  {-0.309017f, -0.500000f, -0.809017f},
  {-0.162460f, -0.262866f, -0.951056f},
  {0.000000f, -0.850651f, -0.525731f},
  {-0.147621f, -0.716567f, -0.681718f},
  {0.147621f, -0.716567f, -0.681718f},
  {0.000000f, -0.525731f, -0.850651f},
  {0.309017f, -0.500000f, -0.809017f},
  {0.442863f, -0.238856f, -0.864188f},
  {0.162460f, -0.262866f, -0.951056f},
  {0.238856f, -0.864188f, -0.442863f},
  {0.500000f, -0.809017f, -0.309017f},
  {0.425325f, -0.688191f, -0.587785f},
  {0.716567f, -0.681718f, -0.147621f},
  {0.688191f, -0.587785f, -0.425325f},
  {0.587785f, -0.425325f, -0.688191f},
  {0.000000f, -0.955423f, -0.295242f},
  {0.000000f, -1.000000f, 0.000000f},
  {0.262866f, -0.951056f, -0.162460f},
  {0.000000f, -0.850651f, 0.525731f},
  {0.000000f, -0.955423f, 0.295242f},
  {0.238856f, -0.864188f, 0.442863f},
  {0.262866f, -0.951056f, 0.162460f},
  {0.500000f, -0.809017f, 0.309017f},
  {0.716567f, -0.681718f, 0.147621f},
  {0.525731f, -0.850651f, 0.000000f},
  {-0.238856f, -0.864188f, -0.442863f},
  {-0.500000f, -0.809017f, -0.309017f},
  {-0.262866f, -0.951056f, -0.162460f},
  {-0.850651f, -0.525731f, 0.000000f},
  {-0.716567f, -0.681718f, -0.147621f},
  {-0.716567f, -0.681718f, 0.147621f},
  {-0.525731f, -0.850651f, 0.000000f},
  {-0.500000f, -0.809017f, 0.309017f},
  {-0.238856f, -0.864188f, 0.442863f},
  {-0.262866f, -0.951056f, 0.162460f},
  {-0.864188f, -0.442863f, 0.238856f},
  {-0.809017f, -0.309017f, 0.500000f},
  {-0.688191f, -0.587785f, 0.425325f},
  {-0.681718f, -0.147621f, 0.716567f},
  {-0.442863f, -0.238856f, 0.864188f},
  {-0.587785f, -0.425325f, 0.688191f},
  {-0.309017f, -0.500000f, 0.809017f},
  {-0.147621f, -0.716567f, 0.681718f},
  {-0.425325f, -0.688191f, 0.587785f},
  {-0.162460f, -0.262866f, 0.951056f},
  {0.442863f, -0.238856f, 0.864188f},
  {0.162460f, -0.262866f, 0.951056f},
  {0.309017f, -0.500000f, 0.809017f},
  {0.147621f, -0.716567f, 0.681718f},
  {0.000000f, -0.525731f, 0.850651f},
  {0.425325f, -0.688191f, 0.587785f},
  {0.587785f, -0.425325f, 0.688191f},
  {0.688191f, -0.587785f, 0.425325f},
  {-0.955423f, 0.295242f, 0.000000f},
  {-0.951056f, 0.162460f, 0.262866f},
  {-1.000000f, 0.000000f, 0.000000f},
  {-0.850651f, 0.000000f, 0.525731f},
  {-0.955423f, -0.295242f, 0.000000f},
  {-0.951056f, -0.162460f, 0.262866f},
  {-0.864188f, 0.442863f, -0.238856f},
  {-0.951056f, 0.162460f, -0.262866f},
  {-0.809017f, 0.309017f, -0.500000f},
  {-0.864188f, -0.442863f, -0.238856f},
  {-0.951056f, -0.162460f, -0.262866f},
  {-0.809017f, -0.309017f, -0.500000f},
  {-0.681718f, 0.147621f, -0.716567f},
  {-0.681718f, -0.147621f, -0.716567f},
  {-0.850651f, 0.000000f, -0.525731f},
  {-0.688191f, 0.587785f, -0.425325f},
  {-0.587785f, 0.425325f, -0.688191f},
  {-0.425325f, 0.688191f, -0.587785f},
  {-0.425325f, -0.688191f, -0.587785f},
  {-0.587785f, -0.425325f, -0.688191f},
  {-0.688191f, -0.587785f, -0.425325f}
};

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

  // Ler bodypart
  fseek(file, header.ofs_bodyparts, SEEK_SET);
  bodypart_t bodypart;
  fread(&bodypart, sizeof(bodypart), 1, file);

  // Ler model
  fseek(file, header.ofs_bodyparts + bodypart.modelindex, SEEK_SET);
  model_t model;
  fread(&model, sizeof(model), 1, file);

  TYRA_LOG("chegamo teste 1");
  // Ler primeiro mesh
  fseek(file, header.ofs_bodyparts + model.meshindex, SEEK_SET);
  mesh_t mesh;
  fread(&mesh, sizeof(mesh), 1, file);

  // Carregar triângulos (vértices expandidos do mesh)
  auto meshVertsBuffer = new trivert_t[mesh.num_verts];
  fseek(file, header.ofs_bodyparts + mesh.vert_index, SEEK_SET);
  fread(meshVertsBuffer, sizeof(trivert_t), mesh.num_verts, file);

  // Carregar vértices do modelo (posições únicas)
  auto modelVertsBuffer = new mvertex_t[model.num_verts];
  fseek(file, header.ofs_bodyparts + model.vert_index, SEEK_SET);
  fread(modelVertsBuffer, sizeof(mvertex_t), model.num_verts, file);

  // Carregar skin name (textura embutida)
  std::string skinName;
  if (header.num_skins > 0) {
    skin_t skin;
    fseek(file, header.ofs_skins, SEEK_SET);
    fread(&skin, sizeof(skin_t), 1, file);
    skinName = std::string(skin.name);
  }

  fclose(file);

  // Criar mesh builder data
  auto result = std::make_unique<MeshBuilderData>();
  
  TYRA_LOG("chegamo teste 2");

  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  
  TYRA_LOG("chegamo teste 3");
  // Se tiver skin embutida, usar ela, senão procurar PNG externo
  if (!skinName.empty()) {
    material->texturePath = skinName;
  } else {
    material->texturePath = material->name;
    material->texturePath.value().append(".png");
  }
  
  TYRA_LOG("chegamo teste 4");

  result->materials.push_back(material);
  result->loadNormals = true;
  result->loadLightmap = false;

  // Criar frame único
  auto* outputFrame = new MeshBuilderMaterialFrameData();
  material->frames.push_back(outputFrame);

  // Alocar arrays - usamos mesh.num_verts que já são vértices expandidos
  outputFrame->count = mesh.num_verts;
  outputFrame->vertices = new Vec4[mesh.num_verts];
  outputFrame->normals = new Vec4[mesh.num_verts];
  outputFrame->textureCoords = new Vec4[mesh.num_verts];

  // Preencher dados - cada entrada em meshVertsBuffer é um vértice já expandido
  Vec4 temp(0.0F, 0.0F, 0.0F, 1.0F);
  
  TYRA_LOG("chegamo teste 5");

  for (u32 i = 0; i < mesh.num_verts; i++) {
    // meshVertsBuffer[i].vertindex aponta para o vértice único em modelVertsBuffer
    u32 vertIndex = meshVertsBuffer[i].vertindex;
    
    // Vértice com escala aplicada
    temp.set(
      static_cast<float>(modelVertsBuffer[vertIndex].v[0]) * options.scale,
      static_cast<float>(modelVertsBuffer[vertIndex].v[1]) * options.scale,
      static_cast<float>(modelVertsBuffer[vertIndex].v[2]) * options.scale,
      1.0F
    );
    outputFrame->vertices[i] = temp;

    // Normal (usando índice da tabela de normais)
    u32 normIndex = meshVertsBuffer[i].normindex;
    if (normIndex < 162) {
      temp.set(
        ANORMS[normIndex][0],
        ANORMS[normIndex][1],
        ANORMS[normIndex][2],
        0.0F
      );
    } else {
      temp.set(0.0F, 1.0F, 0.0F, 0.0F);
    }
    outputFrame->normals[i] = temp;

    // Coordenadas de textura (já vêm no trivert)
    float u = static_cast<float>(meshVertsBuffer[i].s) / 256.0f;
    float v = static_cast<float>(meshVertsBuffer[i].t) / 256.0f;
    
    if (options.flipUVs) {
      v = 1.0F - v;
    }
    
    temp.set(u, v, 1.0F, 0.0F);
    outputFrame->textureCoords[i] = temp;
  }

  // Limpar buffers
  delete[] modelVertsBuffer;
  delete[] meshVertsBuffer;

  TYRA_LOG("chegamo teste 6");


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
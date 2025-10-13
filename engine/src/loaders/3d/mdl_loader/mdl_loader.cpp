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

#define IDSTUDIOHEADER (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I') // "IDST"
#define STUDIO_VERSION 10

// Header principal do MDL do GoldSrc (Half-Life 1)
typedef struct {
  int id;                    // "IDST"
  int version;               // Versão (10 para HL1)
  
  char name[64];             // Nome do modelo
  int length;                // Tamanho do arquivo
  
  vec3_t eyeposition;        // Posição ideal dos olhos
  vec3_t min;                // Bounding box mínimo
  vec3_t max;                // Bounding box máximo
  
  vec3_t bbmin;              // Clipping box mínimo
  vec3_t bbmax;              // Clipping box máximo
  
  int flags;                 // Flags do modelo
  
  int numbones;              // Número de ossos
  int boneindex;             // Offset para ossos
  
  int numbonecontrollers;    // Número de controladores de ossos
  int bonecontrollerindex;   // Offset para controladores
  
  int numhitboxes;           // Número de hitboxes
  int hitboxindex;           // Offset para hitboxes
  
  int numseq;                // Número de sequências de animação
  int seqindex;              // Offset para sequências
  
  int numseqgroups;          // Número de grupos de sequência
  int seqgroupindex;         // Offset para grupos
  
  int numtextures;           // Número de texturas
  int textureindex;          // Offset para texturas
  int texturedataindex;      // Offset para dados de textura
  
  int numskinref;            // Número de referências de skin
  int numskinfamilies;       // Número de famílias de skin
  int skinindex;             // Offset para skins
  
  int numbodyparts;          // Número de partes do corpo
  int bodypartindex;         // Offset para partes do corpo
  
  int numattachments;        // Número de pontos de anexo
  int attachmentindex;       // Offset para anexos
  
  int soundtable;
  int soundindex;
  int soundgroups;
  int soundgroupindex;
  
  int numtransitions;
  int transitionindex;
} studiohdr_t;

typedef vec3_t vec3_array_t[3];

typedef struct {
  char name[64];
  int nummodels;
  int base;
  int modelindex;
} mstudiobodyparts_t;

typedef struct {
  char name[64];
  int type;
  
  float boundingradius;
  
  int nummesh;
  int meshindex;
  
  int numverts;
  int vertinfoindex;
  int vertindex;
  
  int numnorms;
  int norminfoindex;
  int normindex;
  
  int numgroups;
  int groupindex;
} mstudiomodel_t;

typedef struct {
  int numtris;
  int triindex;
  int skinref;
  int numnorms;
  int normindex;
} mstudiomesh_t;

typedef struct {
  short vertindex;
} mstudiotrivert_t;

std::unique_ptr<MeshBuilderData> MDLLoader::load(const char* fullpath,
                                                 MDLLoaderOptions options) {
  std::string path = fullpath;
  TYRA_ASSERT(!path.empty(), "Provided path is empty!");

  auto filename = FileUtils::getFilenameFromPath(path);

  FILE* file = fopen(fullpath, "rb");
  TYRA_ASSERT(file != nullptr, "Failed to load: ", filename);
  
  // Ler header
  studiohdr_t header;
  fread(reinterpret_cast<char*>(&header), sizeof(studiohdr_t), 1, file);

  TYRA_LOG("MDL ID: 0x", std::hex, header.id, std::dec);
  TYRA_LOG("MDL Version: ", header.version);
  
  TYRA_ASSERT(header.id == IDSTUDIOHEADER, "Invalid MDL file - wrong magic number!");
  TYRA_ASSERT(header.version == STUDIO_VERSION, "Invalid MDL version - expected 10, got ", header.version);

  TYRA_LOG("MDL: ", header.name);
  TYRA_LOG("  Bodyparts: ", header.numbodyparts);
  TYRA_LOG("  Textures: ", header.numtextures);
  TYRA_LOG("  Bones: ", header.numbones);

  TYRA_ASSERT(header.numbodyparts > 0, "MDL has no bodyparts!");

  // Ler primeira bodypart
  fseek(file, header.bodypartindex, SEEK_SET);
  mstudiobodyparts_t bodypart;
  fread(&bodypart, sizeof(mstudiobodyparts_t), 1, file);
  
  TYRA_LOG("Bodypart: ", bodypart.name);
  TYRA_LOG("  Models: ", bodypart.nummodels);
  
  TYRA_ASSERT(bodypart.nummodels > 0, "Bodypart has no models!");

  // Ler primeiro model
  fseek(file, header.bodypartindex + bodypart.modelindex, SEEK_SET);
  mstudiomodel_t model;
  fread(&model, sizeof(mstudiomodel_t), 1, file);
  
  TYRA_LOG("Model: ", model.name);
  TYRA_LOG("  Meshes: ", model.nummesh);
  TYRA_LOG("  Verts: ", model.numverts);
  
  TYRA_ASSERT(model.nummesh > 0, "Model has no meshes!");
  TYRA_ASSERT(model.numverts > 0, "Model has no vertices!");

  // Ler primeiro mesh
  fseek(file, header.bodypartindex + bodypart.modelindex + model.meshindex, SEEK_SET);
  mstudiomesh_t mesh;
  fread(&mesh, sizeof(mstudiomesh_t), 1, file);
  
  TYRA_LOG("Mesh:");
  TYRA_LOG("  Tris: ", mesh.numtris);
  TYRA_LOG("  Norms: ", mesh.numnorms);
  
  TYRA_ASSERT(mesh.numtris > 0, "Mesh has no triangles!");

  // Ler vértices do modelo
  auto verts = new vec3_t[model.numverts];
  fseek(file, header.bodypartindex + bodypart.modelindex + model.vertindex, SEEK_SET);
  fread(verts, sizeof(vec3_t), model.numverts, file);

  // Ler índices de triângulos
  // GoldSrc usa triangle strips/fans, vamos ler como array simples por enquanto
  auto triVerts = new mstudiotrivert_t[mesh.numtris * 3];
  fseek(file, header.bodypartindex + bodypart.modelindex + model.meshindex + mesh.triindex, SEEK_SET);
  fread(triVerts, sizeof(mstudiotrivert_t), mesh.numtris * 3, file);

  fclose(file);

  // Criar mesh builder data
  auto result = std::make_unique<MeshBuilderData>();
  
  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  material->texturePath = material->name;
  material->texturePath.value().append(".bmp");  // GoldSrc geralmente usa BMP
  
  result->materials.push_back(material);
  result->loadNormals = false;
  result->loadLightmap = false;

  // Criar frame
  auto* outputFrame = new MeshBuilderMaterialFrameData();
  material->frames.push_back(outputFrame);

  outputFrame->count = mesh.numtris * 3;
  outputFrame->vertices = new Vec4[mesh.numtris * 3];

  TYRA_LOG("Creating mesh with ", mesh.numtris, " triangles");

  // Preencher vértices
  for (int i = 0; i < mesh.numtris * 3; i++) {
    int vertIdx = triVerts[i].vertindex;
    
    if (vertIdx >= 0 && vertIdx < model.numverts) {
      outputFrame->vertices[i].set(
        verts[vertIdx][0] * options.scale,
        verts[vertIdx][1] * options.scale,
        verts[vertIdx][2] * options.scale,
        1.0f
      );
    } else {
      TYRA_LOG("Warning: invalid vertex index ", vertIdx);
      outputFrame->vertices[i].set(0.0f, 0.0f, 0.0f, 1.0f);
    }
  }

  delete[] verts;
  delete[] triVerts;
  
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
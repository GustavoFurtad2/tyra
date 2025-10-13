#include "loaders/3d/mdl_loader/mdl_loader.hpp"
#include <stdio.h>
#include <string>
#include "debug/debug.hpp"
#include "file/file_utils.hpp"

namespace Tyra {

#define IDSTUDIOHEADER (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I') // "IDST"
#define STUDIO_VERSION 10

// Estruturas do MDL GoldSrc
typedef struct {
  int id;
  int version;
  char name[64];
  int length;
  vec3_t eyeposition;
  vec3_t min;
  vec3_t max;
  vec3_t bbmin;
  vec3_t bbmax;
  int flags;
  int numbones;
  int boneindex;
  int numbonecontrollers;
  int bonecontrollerindex;
  int numhitboxes;
  int hitboxindex;
  int numseq;
  int seqindex;
  int numseqgroups;
  int seqgroupindex;
  int numtextures;
  int textureindex;
  int texturedataindex;
  int numskinref;
  int numskinfamilies;
  int skinindex;
  int numbodyparts;
  int bodypartindex;
  int numattachments;
  int attachmentindex;
  int soundtable;
  int soundindex;
  int soundgroups;
  int soundgroupindex;
  int numtransitions;
  int transitionindex;
} studiohdr_t;

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

std::unique_ptr<MeshBuilderData> MDLLoader::load(const char* fullpath, MDLLoaderOptions options) {
  std::string path = fullpath;
  TYRA_ASSERT(!path.empty(), "Provided path is empty!");

  auto filename = FileUtils::getFilenameFromPath(path);
  FILE* file = fopen(fullpath, "rb");
  TYRA_ASSERT(file != nullptr, "Failed to load: ", filename);

  // Ler header
  studiohdr_t header;
  fread(&header, sizeof(studiohdr_t), 1, file);
  TYRA_ASSERT(header.id == IDSTUDIOHEADER, "Invalid MDL file - wrong magic number!");
  TYRA_ASSERT(header.version == STUDIO_VERSION, "Invalid MDL version - expected 10, got ", header.version);

  // Ler bodypart
  fseek(file, header.bodypartindex, SEEK_SET);
  mstudiobodyparts_t bodypart;
  fread(&bodypart, sizeof(mstudiobodyparts_t), 1, file);
  TYRA_ASSERT(bodypart.nummodels > 0, "Bodypart has no models!");

  // Ler model
  fseek(file, bodypart.modelindex, SEEK_SET);
  mstudiomodel_t model;
  fread(&model, sizeof(mstudiomodel_t), 1, file);
  TYRA_ASSERT(model.nummesh > 0, "Model has no meshes!");
  TYRA_ASSERT(model.numverts > 0, "Model has no vertices!");

  // Ler mesh
  fseek(file, model.meshindex, SEEK_SET);
  mstudiomesh_t mesh;
  fread(&mesh, sizeof(mstudiomesh_t), 1, file);
  TYRA_ASSERT(mesh.numtris > 0, "Mesh has no triangles!");

  // Ler vértices
  auto verts = new vec3_t[model.numverts];
  fseek(file, model.vertindex, SEEK_SET);
  fread(verts, sizeof(vec3_t), model.numverts, file);

  // Ler triângulos
  auto triVerts = new mstudiotrivert_t[mesh.numtris * 3];
  fseek(file, mesh.triindex, SEEK_SET);
  fread(triVerts, sizeof(mstudiotrivert_t), mesh.numtris * 3, file);

  fclose(file);

  // Criar MeshBuilderData
  auto result = std::make_unique<MeshBuilderData>();
  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  material->texturePath = material->name + ".bmp"; // GoldSrc geralmente usa BMP
  result->materials.push_back(material);
  result->loadNormals = false;
  result->loadLightmap = false;

  // Criar frame
  auto* outputFrame = new MeshBuilderMaterialFrameData();
  material->frames.push_back(outputFrame);
  outputFrame->count = mesh.numtris * 3;
  outputFrame->vertices = new Vec4[mesh.numtris * 3];

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

}  // namespace Tyra

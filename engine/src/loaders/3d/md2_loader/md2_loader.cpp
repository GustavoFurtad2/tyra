/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "loaders/3d/md2_loader/md2_loader.hpp"
#include <stdio.h>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "debug/debug.hpp"
#include "loaders/3d/md2_loader/anorms.hpp"
#include "file/file_utils.hpp"

namespace Tyra {

// magic number "IDP2" or 844121161
#define MD2_IDENT (('2' << 24) + ('P' << 16) + ('D' << 8) + 'I')

// model version
#define MD2_VERSION 8

typedef struct {
  int ident;    // magic number. must be equal to "IDP2"
  int version;  // md2 version. must be equal to 8

  int skinwidth;   // width of the texture
  int skinheight;  // height of the texture
  int framesize;   // size of one frame in bytes

  int num_skins;   // number of textures
  int num_xyz;     // number of vertices
  int num_st;      // number of texture coordinates
  int num_tris;    // number of triangles
  int num_glcmds;  // number of opengl commands
  int num_frames;  // total number of frames

  int ofs_skins;   // offset to skin names (64 bytes each)
  int ofs_st;      // offset to s-t texture coordinates
  int ofs_tris;    // offset to triangles
  int ofs_frames;  // offset to frame data
  int ofs_glcmds;  // offset to opengl commands
  int ofs_end;     // offset to end of file
} md2_t;

typedef struct {
  unsigned char v[3];              // compressed vertex (x, y, z) coordinates
  unsigned char lightnormalindex;  // index to a normal vector for the lighting
} mvertex_t;

typedef struct {
  float scale[3];      // scale values
  float translate[3];  // translation vector
  char name[16];       // frame name
  mvertex_t verts[1];  // first vertex of this frame
} frame_t;

typedef float vec3_t[3];

typedef struct {
  s16 index_xyz[3];  // indexes to triangle's vertices
  s16 index_st[3];   // indexes to vertices' texture coorinates
} triangle_t;

typedef struct {
  s16 s;
  s16 t;
} texCoord_t;

std::unique_ptr<MeshBuilderData> MD2Loader::load(const char* fullpath) {
  return load(fullpath, MD2LoaderOptions());
}

std::unique_ptr<MeshBuilderData> MD2Loader::load(const std::string& fullpath) {
  return load(fullpath.c_str(), MD2LoaderOptions());
}

std::unique_ptr<MeshBuilderData> MD2Loader::load(const std::string& fullpath,
                                                 MD2LoaderOptions options) {
  return load(fullpath.c_str(), options);
}

// Helper function to check if value is valid float
bool isValidFloat(float value) {
  return !std::isnan(value) && !std::isinf(value) && 
         value > -1000000.0f && value < 1000000.0f;
}

// Helper function to clamp float values
float clampFloat(float value, float min_val = -10000.0f, float max_val = 10000.0f) {
  if (!isValidFloat(value)) return 0.0f;
  return std::max(min_val, std::min(max_val, value));
}

std::unique_ptr<MeshBuilderData> MD2Loader::load(const char* fullpath,
                                                 MD2LoaderOptions options) {
  std::string path = fullpath;
  TYRA_ASSERT(!path.empty(), "Provided path is empty!");

  auto filename = FileUtils::getFilenameFromPath(path);

  FILE* file = fopen(fullpath, "rb");
  TYRA_ASSERT(file != nullptr, "Failed to load: ", filename);
  md2_t header;

  fread(reinterpret_cast<char*>(&header), sizeof(md2_t), 1, file);

  TYRA_ASSERT((header.ident == MD2_IDENT) && (header.version == MD2_VERSION),
              "This MD2 file is not in correct format!");

  // Validação adicional do header
  TYRA_ASSERT(header.skinwidth > 0 && header.skinheight > 0,
              "Invalid skin dimensions: ", header.skinwidth, "x", header.skinheight);
  
  TYRA_ASSERT(header.num_frames > 0 && header.num_frames < 10000,
              "Invalid frame count: ", header.num_frames);
  
  TYRA_ASSERT(header.num_xyz > 0 && header.num_xyz < 100000,
              "Invalid vertex count: ", header.num_xyz);

  u32 framesCount = header.num_frames;
  u32 vertexCount = header.num_xyz;
  u32 stsCount = header.num_st;
  u32 trianglesCount = header.num_tris;

  auto framesBufferSize = framesCount * header.framesize;
  
  // Usar malloc para garantir alinhamento de 4 bytes
  auto framesBuffer = static_cast<char*>(malloc(framesBufferSize));
  TYRA_ASSERT(framesBuffer != nullptr, "Failed to allocate frames buffer");
  
  auto stsBuffer = static_cast<char*>(malloc(stsCount * sizeof(texCoord_t)));
  TYRA_ASSERT(stsBuffer != nullptr, "Failed to allocate STs buffer");
  
  auto trianglesBuffer = static_cast<char*>(malloc(trianglesCount * sizeof(triangle_t)));
  TYRA_ASSERT(trianglesBuffer != nullptr, "Failed to allocate triangles buffer");

  // Verificar alinhamento (debug)
  printf("Frames buffer alignment: %zu\n", reinterpret_cast<uintptr_t>(framesBuffer) % 4);
  printf("STs buffer alignment: %zu\n", reinterpret_cast<uintptr_t>(stsBuffer) % 4);
  printf("Triangles buffer alignment: %zu\n", reinterpret_cast<uintptr_t>(trianglesBuffer) % 4);

  fseek(file, header.ofs_frames, SEEK_SET);
  fread(framesBuffer, framesBufferSize, 1, file);

  fseek(file, header.ofs_st, SEEK_SET);
  fread(stsBuffer, stsCount * sizeof(texCoord_t), 1, file);

  fseek(file, header.ofs_tris, SEEK_SET);
  fread(trianglesBuffer, trianglesCount * sizeof(triangle_t), 1, file);

  fclose(file);

  auto result = std::make_unique<MeshBuilderData>();

  auto* material = new MeshBuilderMaterialData();
  material->name = FileUtils::getFilenameWithoutExtension(filename);
  material->texturePath = material->name;
  material->texturePath.value().append(".png");

  result->materials.push_back(material);
  result->loadNormals = true;
  result->loadLightmap = false;

  Vec4** tempVertices = new Vec4*[framesCount];
  Vec4** tempNormals = new Vec4*[framesCount];
  Vec4** tempTexCoords = new Vec4*[framesCount];

  for (u32 i = 0; i < framesCount; i++) {
    auto* outputFrame = new MeshBuilderMaterialFrameData();
    material->frames.push_back(outputFrame);

    tempVertices[i] = new Vec4[vertexCount];
    tempNormals[i] = new Vec4[vertexCount];
    tempTexCoords[i] = new Vec4[stsCount];
  }

  Vec4 temp(0.0F, 0.0F, 0.0F, 1.0F);

  for (u32 frameIndex = 0; frameIndex < framesCount; frameIndex++) {
    // Verificar alinhamento antes do cast
    char* framePtr = &framesBuffer[header.framesize * frameIndex];
    uintptr_t alignment = reinterpret_cast<uintptr_t>(framePtr) % 4;
    
    frame_t alignedFrame;
    frame_t* frame;
    
    if (alignment != 0) {
      // Se não está alinhado, copiar para estrutura alinhada
      printf("Warning: Frame %d not aligned, copying data\n", frameIndex);
      memcpy(&alignedFrame, framePtr, sizeof(frame_t));
      frame = &alignedFrame;
    } else {
      frame = reinterpret_cast<frame_t*>(framePtr);
    }

    // Debug dos valores de scale e translate
    printf("Frame %d - Scale: %.6f %.6f %.6f\n", frameIndex, 
           frame->scale[0], frame->scale[1], frame->scale[2]);
    printf("Frame %d - Translate: %.6f %.6f %.6f\n", frameIndex,
           frame->translate[0], frame->translate[1], frame->translate[2]);

    // Validar valores de scale e translate
    for (int i = 0; i < 3; i++) {
      if (!isValidFloat(frame->scale[i])) {
        printf("Warning: Invalid scale[%d] = %f, setting to 1.0\n", i, frame->scale[i]);
        frame->scale[i] = 1.0f;
      }
      if (!isValidFloat(frame->translate[i])) {
        printf("Warning: Invalid translate[%d] = %f, setting to 0.0\n", i, frame->translate[i]);
        frame->translate[i] = 0.0f;
      }
    }

    for (u32 vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
      float x = ((frame->verts[vertexIndex].v[0] * frame->scale[0]) +
                frame->translate[0]) * options.scale;
      float y = ((frame->verts[vertexIndex].v[1] * frame->scale[1]) +
                frame->translate[1]) * options.scale;
      float z = ((frame->verts[vertexIndex].v[2] * frame->scale[2]) +
                frame->translate[2]) * options.scale;

      // Clamping de valores extremos
      x = clampFloat(x);
      y = clampFloat(y);
      z = clampFloat(z);

      temp.set(x, y, z);
      tempVertices[frameIndex][vertexIndex].set(temp);

      // Verificar índice de normal
      u8 normalIndex = frame->verts[vertexIndex].lightnormalindex;
      if (normalIndex >= 162) { // ANORMS tem 162 entradas
        printf("Warning: Invalid normal index %d, using 0\n", normalIndex);
        normalIndex = 0;
      }

      temp.set(ANORMS[normalIndex][0],
               ANORMS[normalIndex][1],
               ANORMS[normalIndex][2]);

      tempNormals[frameIndex][vertexIndex].set(temp);
    }
  }

  for (u32 i = 0; i < stsCount; i++) {
    char* texCoordPtr = &stsBuffer[sizeof(texCoord_t) * i];
    
    texCoord_t alignedTexCoord;
    texCoord_t* texCoord;
    
    // Verificar alinhamento
    if (reinterpret_cast<uintptr_t>(texCoordPtr) % 4 != 0) {
      memcpy(&alignedTexCoord, texCoordPtr, sizeof(texCoord_t));
      texCoord = &alignedTexCoord;
    } else {
      texCoord = reinterpret_cast<texCoord_t*>(texCoordPtr);
    }

    float s = clampFloat(static_cast<float>(texCoord->s) / header.skinwidth, 0.0f, 1.0f);
    float t = clampFloat(static_cast<float>(texCoord->t) / header.skinheight, 0.0f, 1.0f);

    if (options.flipUVs) t = 1.0F - t;

    temp.set(s, t, 1.0F, 0.0F);

    for (u32 j = 0; j < framesCount; j++) tempTexCoords[j][i].set(temp);
  }

  for (u32 x = 0; x < framesCount; x++) {
    material->frames[x]->count = trianglesCount * 3;
    material->frames[x]->vertices = new Vec4[trianglesCount * 3];
    material->frames[x]->normals = new Vec4[trianglesCount * 3];
    material->frames[x]->textureCoords = new Vec4[trianglesCount * 3];
  }

  for (u32 i = 0; i < trianglesCount; i++) {
    char* trianglePtr = &trianglesBuffer[sizeof(triangle_t) * i];
    
    triangle_t alignedTriangle;
    triangle_t* triangle;
    
    // Verificar alinhamento
    if (reinterpret_cast<uintptr_t>(trianglePtr) % 4 != 0) {
      memcpy(&alignedTriangle, trianglePtr, sizeof(triangle_t));
      triangle = &alignedTriangle;
    } else {
      triangle = reinterpret_cast<triangle_t*>(trianglePtr);
    }

    // Validar índices
    for (u8 j = 0; j < 3; j++) {
      if (triangle->index_xyz[j] >= static_cast<s16>(vertexCount)) {
        printf("Warning: Invalid vertex index %d, clamping to %d\n", 
               triangle->index_xyz[j], vertexCount - 1);
        triangle->index_xyz[j] = static_cast<s16>(vertexCount - 1);
      }
      if (triangle->index_st[j] >= static_cast<s16>(stsCount)) {
        printf("Warning: Invalid ST index %d, clamping to %d\n", 
               triangle->index_st[j], stsCount - 1);
        triangle->index_st[j] = static_cast<s16>(stsCount - 1);
      }
    }

    for (u32 x = 0; x < framesCount; x++) {
      auto* workFrame = material->frames[x];

      for (u8 j = 0; j < 3; j++) {
        workFrame->vertices[i * 3 + j] =
            tempVertices[x][triangle->index_xyz[j]];

        workFrame->normals[i * 3 + j] = tempNormals[x][triangle->index_xyz[j]];

        workFrame->textureCoords[i * 3 + j] =
            tempTexCoords[x][triangle->index_st[j]];
      }
    }
  }

  for (u32 i = 0; i < framesCount; i++) {
    delete[] tempVertices[i];
    delete[] tempNormals[i];
    delete[] tempTexCoords[i];
  }

  delete[] tempVertices;
  delete[] tempNormals;
  delete[] tempTexCoords;

  // Usar free() ao invés de delete[]
  free(framesBuffer);
  free(stsBuffer);
  free(trianglesBuffer);

  printf("MD2 loaded successfully: %s\n", filename.c_str());
  printf("Frames: %d, Vertices: %d, Triangles: %d\n", 
         framesCount, vertexCount, trianglesCount);

  return result;
}

}  // namespace Tyra
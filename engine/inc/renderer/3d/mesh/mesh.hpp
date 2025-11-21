/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#pragma once

#include "math/m4x4.hpp"
#include "./mesh_material.hpp"
#include <tamtypes.h>
#include "debug/debug.hpp"

namespace Tyra {

class Mesh {
 public:
  explicit Mesh(const MeshBuilderData* data);
  Mesh(const Mesh& mesh);
  ~Mesh();

  u8 isMother;

  u32 id;

  M4x4 translation, rotation, scale;

  /** nullptr if not found */
  MeshMaterial* getMaterialByName(const std::string& name);

  std::vector<MeshMaterial*> materials;

  M4x4 getModelMatrix() const;

  /** Get position from translation matrix */
  inline Vec4* getPosition() {
    return reinterpret_cast<Vec4*>(&translation.data[3 * 4]);
  }

  inline Vec4* getAngle() {

    Vec4 angles;

    float sy = -rotation.data[2];

    if (Math::abs(sy) < 0.99999f) {

      angles.x = Math::asin(sy);
      angles.y = Math::atan2(rotation.data[6], rotation.data[10]);
      angles.z = Math::atan2(rotation.data[1], rotation.data[0])
    }
    else {

      angles.x = Math::asin(sy)
      angles.y = Math::atan2(-rotation.data[8], rotation.data[5])
      angles.z = 0.0f;
    }

    angles.w = 1.0f;
    return angles;
  }

  void setPosition(const Vec4& v);

 protected:
  void init();
};

}  // namespace Tyra

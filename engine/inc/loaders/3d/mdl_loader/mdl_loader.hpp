/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Gustavo Furtado <gustav0furt.fatality@gmail.com>
*/

#pragma once

#include "../builder/mesh_builder_data.hpp"
#include <string>
#include <memory>

namespace Tyra {

typedef float vec3_t[3];

struct MDLLoaderOptions {
  bool flipUVs = false;
  float scale = 1.0F;
};

/** Class responsible for loading & parsing GoldSRC's ".mdl" 3D files */
class MDLLoader {
 public:
  static std::unique_ptr<MeshBuilderData> load(const char* fullpath);
  static std::unique_ptr<MeshBuilderData> load(const char* fullpath,
                                               MDLLoaderOptions options);
  static std::unique_ptr<MeshBuilderData> load(const std::string& fullpath);
  static std::unique_ptr<MeshBuilderData> load(const std::string& fullpath,
                                               MDLLoaderOptions options);
};

} // namespace Tyra
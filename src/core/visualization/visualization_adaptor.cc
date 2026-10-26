// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#include <string>
#include <unordered_map>

#include "core/param/param.h"
#include "core/simulation.h"
#include "core/util/log.h"
#include "core/visualization/visualization_adaptor.h"

#include "core/util/shared_library.h"

namespace bdm {

// Factory exported by each adaptor library (see paraview/adaptor.cc).
using AdaptorFactory = VisualizationAdaptor* (*)();

// Adaptor name -> factory, or nullptr when loading failed before. Each library
// is opened only once.
static std::unordered_map<std::string, AdaptorFactory> factories_;

// Adaptor name -> name of the shared library that implements it.
static std::string AdaptorLibrary(const std::string& adaptor) {
  return adaptor == "paraview" ? "VisualizationAdaptor" : "";
}

VisualizationAdaptor* VisualizationAdaptor::Create(const std::string& adaptor) {
  auto* param = Simulation::GetActive()->GetParam();
  if (!(param->insitu_visualization || param->export_visualization)) {
    return nullptr;
  }
  auto it = factories_.find(adaptor);
  if (it == factories_.end()) {
    AdaptorFactory factory = nullptr;
    const std::string library = AdaptorLibrary(adaptor);
    if (library.empty()) {
      Log::Error("VisualizationAdaptor::Create",
                 "Unknown visualization adaptor '", adaptor, "'.");
    } else {
      std::string error;
      factory = reinterpret_cast<AdaptorFactory>(FindSymbolInSharedLibrary(
          library, "BdmCreateVisualizationAdaptor", &error));
      if (factory == nullptr) {
        Log::Error("VisualizationAdaptor::Create",
                   "Was unable to load plugin '", adaptor, "': ", error);
      } else {
        Log::Info("VisualizationAdaptor::Create", "Loaded plugin '", adaptor,
                  "' successfully!");
      }
    }
    it = factories_.emplace(adaptor, factory).first;
  }
  return it->second != nullptr ? it->second() : nullptr;
}

}  // namespace bdm

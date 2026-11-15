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
//
// Modified in the BioDynaMo skunkworks fork by markomanka, 2026
// (https://github.com/markomanka/biodynamo); see its git history.

#include <dlfcn.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/param/param.h"
#include "core/simulation.h"
#include "core/util/log.h"
#include "core/visualization/visualization_adaptor.h"

#include "TPluginManager.h"
#include "TROOT.h"

namespace bdm {

namespace {

// Factories registered with VisualizationAdaptor::Register. A function-local
// static, because libraries register from their static initializers.
std::unordered_map<std::string, VisualizationAdaptor::Factory>& Factories() {
  static std::unordered_map<std::string, VisualizationAdaptor::Factory>
      factories;
  return factories;
}

// Loads the library that provides the ParaView adaptor, which registers
// itself while loading. The library is looked for next to libbiodynamo first,
// where both the build and the installation put it, then by name alone, which
// leaves the search to the dynamic loader. Returns false, with the loader's
// messages in `errors`, if it cannot be loaded.
//
// This used to go through ROOT's plugin manager, which creates the adaptor by
// calling its constructor through ROOT's interpreter. For that the interpreter
// parses the adaptor's headers, which fails on macOS with an SDK newer than
// 14 (conda-forge/root-feedstock#396).
bool LoadParaviewAdaptorLibrary(std::string* errors) {
#ifdef __APPLE__
  const std::string file = "libVisualizationAdaptor.dylib";
#else
  const std::string file = "libVisualizationAdaptor.so";
#endif
  std::vector<std::string> candidates;
  static const int anchor = 0;
  Dl_info info;
  if (dladdr(&anchor, &info) != 0 && info.dli_fname != nullptr) {
    std::string self = info.dli_fname;
    auto slash = self.find_last_of('/');
    if (slash != std::string::npos) {
      candidates.push_back(self.substr(0, slash + 1) + file);
    }
  }
  candidates.push_back(file);
  for (auto& candidate : candidates) {
    // As ROOT's own loading did: bind lazily, and make the symbols visible to
    // libraries loaded later (ParaView's Python modules need Python's).
    if (dlopen(candidate.c_str(), RTLD_LAZY | RTLD_GLOBAL) != nullptr) {
      return true;
    }
    const char* error = dlerror();
    *errors += Concat("\n  ", error != nullptr ? error : candidate.c_str());
  }
  return false;
}

// Adaptors other than the registered ones: ROOT's plugin handlers, found in
// etc/plugins. Keeps track of whether plugins (the keys) have been loaded,
// and of their plugin handler (the values), so that a shared library is not
// loaded more than once.
VisualizationAdaptor* CreateFromRootPlugin(const std::string& adaptor) {
  static std::unordered_map<std::string, TPluginHandler*> loaded;
  if (loaded.count(adaptor) != 0) {
    auto* handler = loaded[adaptor];
    if (handler == nullptr) {  // loading failed before
      return nullptr;
    }
    return reinterpret_cast<VisualizationAdaptor*>(handler->ExecPlugin(0));
  }
  auto* handler =
      gPluginMgr->FindHandler("VisualizationAdaptor", adaptor.c_str());
  if (handler == nullptr) {
    Log::Error(
        "VisualizationAdaptor::Create", "Unable to find plugin '", adaptor,
        "'. This is most likely because bdm.rootrc was not read properly.");
    return nullptr;
  }
  if (handler->LoadPlugin() != 0) {
    loaded[adaptor] = nullptr;
    Log::Error("VisualizationAdaptor::Create", "Was unable to load plugin '",
               adaptor, "'!");
    return nullptr;
  }
  loaded[adaptor] = handler;
  return reinterpret_cast<VisualizationAdaptor*>(handler->ExecPlugin(0));
}

}  // namespace

void VisualizationAdaptor::Register(const std::string& name, Factory factory) {
  Factories()[name] = factory;
}

VisualizationAdaptor* VisualizationAdaptor::Create(const std::string& adaptor) {
  auto* param = Simulation::GetActive()->GetParam();
  if (!(param->insitu_visualization || param->export_visualization)) {
    return nullptr;
  }
  auto& factories = Factories();
  auto it = factories.find(adaptor);
  if (it == factories.end() && adaptor == "paraview") {
    // Not registered yet: the library has not been loaded. Try once.
    static bool tried = false;
    if (!tried) {
      tried = true;
      std::string errors;
      if (!LoadParaviewAdaptorLibrary(&errors)) {
        Log::Error("VisualizationAdaptor::Create",
                   "Could not load the ParaView adaptor library:", errors);
      }
      it = factories.find(adaptor);
    }
    if (it == factories.end()) {
      Log::Error("VisualizationAdaptor::Create",
                 "The ParaView adaptor is not available. BioDynaMo must be "
                 "built with ParaView (-Dparaview=ON) to visualize with it.");
      return nullptr;
    }
  }
  if (it != factories.end()) {
    return it->second();
  }
  return CreateFromRootPlugin(adaptor);
}

}  // namespace bdm

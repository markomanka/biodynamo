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

#ifndef CORE_VIRTUALIZATION_VIRTUALIZATION_ADAPTOR_H_
#define CORE_VIRTUALIZATION_VIRTUALIZATION_ADAPTOR_H_

#include <string>

namespace bdm {

class VisualizationAdaptor {
 public:
  /// Creates one adaptor; registered under a name with `Register`.
  using Factory = VisualizationAdaptor* (*)();

  VisualizationAdaptor() = default;

  /// Returns a new adaptor with the given name, or nullptr if visualization
  /// is switched off or no such adaptor exists. Adaptors registered with
  /// `Register` are created directly. "paraview" is registered by
  /// libVisualizationAdaptor, which is loaded on first use. Any other name
  /// is looked up among ROOT's plugin handlers, as before.
  static VisualizationAdaptor* Create(const std::string& adaptor);

  /// Makes `Create(name)` return what `factory` creates. A library that
  /// provides an adaptor calls this when it is loaded; registering a name
  /// again replaces the earlier factory.
  static void Register(const std::string& name, Factory factory);

  virtual ~VisualizationAdaptor() = default;

  // To be implemented by the adaptor
  virtual void Visualize() = 0;
};

}  // namespace bdm

#endif  // CORE_VIRTUALIZATION_VIRTUALIZATION_ADAPTOR_H_

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
// Added in the BioDynaMo skunkworks fork by markomanka, 2026
// (https://github.com/markomanka/biodynamo), to state how a visualization
// adaptor is registered and created without ROOT's plugin manager.

#include "core/visualization/visualization_adaptor.h"

#include <gtest/gtest.h>

#include <memory>

#include "core/param/param.h"
#include "core/simulation.h"
#include "unit/test_util/test_util.h"

namespace bdm {
namespace visualization_adaptor_test_internal {

struct FirstAdaptor : public VisualizationAdaptor {
  void Visualize() override {}
};

struct SecondAdaptor : public VisualizationAdaptor {
  void Visualize() override {}
};

VisualizationAdaptor* NewFirstAdaptor() { return new FirstAdaptor(); }
VisualizationAdaptor* NewSecondAdaptor() { return new SecondAdaptor(); }

// The engine is set to the test's own adaptor, so that the simulation does not
// look for ParaView.
void ExportWith(Param* param, const std::string& engine) {
  param->export_visualization = true;
  param->visualization_export_generate_pvsm = false;
  param->visualization_engine = engine;
}

TEST(VisualizationAdaptorTest, CreatesRegisteredAdaptor) {
  VisualizationAdaptor::Register("adaptor-test-first", &NewFirstAdaptor);
  Simulation simulation(
      TEST_NAME, [](Param* param) { ExportWith(param, "adaptor-test-first"); });

  std::unique_ptr<VisualizationAdaptor> first(
      VisualizationAdaptor::Create("adaptor-test-first"));
  std::unique_ptr<VisualizationAdaptor> second(
      VisualizationAdaptor::Create("adaptor-test-first"));
  ASSERT_NE(nullptr, first.get());
  ASSERT_NE(nullptr, second.get());
  EXPECT_NE(nullptr, dynamic_cast<FirstAdaptor*>(first.get()));
  // Every call creates a new adaptor.
  EXPECT_NE(first.get(), second.get());
}

TEST(VisualizationAdaptorTest, CreatesNothingWhenVisualizationIsOff) {
  VisualizationAdaptor::Register("adaptor-test-first", &NewFirstAdaptor);
  Simulation simulation(TEST_NAME);

  EXPECT_EQ(nullptr, VisualizationAdaptor::Create("adaptor-test-first"));
}

TEST(VisualizationAdaptorTest, RegisteringAgainReplacesTheFactory) {
  VisualizationAdaptor::Register("adaptor-test-replaced", &NewFirstAdaptor);
  VisualizationAdaptor::Register("adaptor-test-replaced", &NewSecondAdaptor);
  Simulation simulation(TEST_NAME, [](Param* param) {
    ExportWith(param, "adaptor-test-replaced");
  });

  std::unique_ptr<VisualizationAdaptor> adaptor(
      VisualizationAdaptor::Create("adaptor-test-replaced"));
  ASSERT_NE(nullptr, adaptor.get());
  EXPECT_NE(nullptr, dynamic_cast<SecondAdaptor*>(adaptor.get()));
}

}  // namespace visualization_adaptor_test_internal
}  // namespace bdm

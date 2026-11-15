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
// (https://github.com/markomanka/biodynamo), to state what the ParaView export
// of agents must produce, independently of how it is implemented.

#ifndef UNIT_CORE_VISUALIZATION_PARAVIEW_EXPORT_CONTRACT_TEST_H_
#define UNIT_CORE_VISUALIZATION_PARAVIEW_EXPORT_CONTRACT_TEST_H_

#include <cstdint>

#include "core/agent/agent_pointer.h"
#include "core/agent/agent_uid.h"
#include "core/agent/cell.h"
#include "core/container/math_array.h"
#include "core/real_t.h"

namespace bdm {
namespace export_contract_test_internal {

/// An agent with one data member of each type that the ParaView export
/// supports, besides those it inherits from Cell and Agent. The members are
/// public so that the test can set and compare them directly.
class ExportContractAgent : public Cell {
  BDM_AGENT_HEADER(ExportContractAgent, Cell, 1);

 public:
  ExportContractAgent() = default;
  virtual ~ExportContractAgent() = default;

  int int_field_ = 0;
  uint64_t uint64_field_ = 0;
  real_t real_field_ = 0;
  Real3 real3_field_ = {0, 0, 0};
  Real4 real4_field_ = {0, 0, 0, 0};
  AgentUid uid_field_;
  AgentPointer<Cell> pointer_field_;
};

/// An agent with a data member of a type that the export does not support.
class UnsupportedFieldAgent : public Cell {
  BDM_AGENT_HEADER(UnsupportedFieldAgent, Cell, 1);

 public:
  UnsupportedFieldAgent() = default;
  virtual ~UnsupportedFieldAgent() = default;

  bool bool_field_ = false;
};

}  // namespace export_contract_test_internal
}  // namespace bdm

#endif  // UNIT_CORE_VISUALIZATION_PARAVIEW_EXPORT_CONTRACT_TEST_H_

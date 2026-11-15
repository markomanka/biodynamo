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
//
// These tests look only at the exported files: which arrays they hold, with
// how many components and of which numeric type, and the values, compared with
// the agents they came from. They do not depend on how the export finds the
// data members or turns them into arrays, so they stay valid when ROOT's
// interpreter, and later ROOT's dictionaries, are replaced. Only
// ReadExportedAgents knows the file format, and is the one function to change
// when the export moves to another format.

#ifdef USE_PARAVIEW

#include "unit/core/visualization/paraview/export_contract_test.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPUnstructuredGridReader.h>

#include "biodynamo.h"
#include "unit/test_util/test_util.h"

namespace bdm {
namespace export_contract_test_internal {

// -----------------------------------------------------------------------------
/// Numeric types an exported array can have, independently of how VTK names
/// them on a given platform (uint64_t is `unsigned long` on Linux and
/// `unsigned long long` on macOS).
enum class Kind { kReal, kInt32, kUInt64 };

bool HasKind(vtkDataArray* array, Kind kind) {
  int type = array->GetDataType();
  switch (kind) {
    case Kind::kReal:
      return sizeof(real_t) == 8 ? type == VTK_DOUBLE : type == VTK_FLOAT;
    case Kind::kInt32:
      return type == VTK_INT && array->GetDataTypeSize() == 4;
    case Kind::kUInt64:
      return (type == VTK_UNSIGNED_LONG || type == VTK_UNSIGNED_LONG_LONG) &&
             array->GetDataTypeSize() == 8;
  }
  return false;
}

// -----------------------------------------------------------------------------
/// One array that the export must write: its name, the number of components,
/// the numeric type, and the values it must hold for a given agent. Every
/// value used in these tests is exactly representable as a double.
struct ExpectedArray {
  std::string name;
  int components;
  Kind kind;
  std::vector<double> (*values)(const ExportContractAgent&);
};

double U64(uint64_t value) { return static_cast<double>(value); }

const std::vector<ExpectedArray>& ExpectedArrays() {
  using A = ExportContractAgent;
  static const std::vector<ExpectedArray> expected = {
      // Inherited from Agent and Cell.
      {"uid_", 1, Kind::kUInt64,
       [](const A& a) { return std::vector<double>{U64(a.GetUid())}; }},
      {"diameter_", 1, Kind::kReal,
       [](const A& a) { return std::vector<double>{a.GetDiameter()}; }},
      // Declared by ExportContractAgent, one of each supported type.
      {"int_field_", 1, Kind::kInt32,
       [](const A& a) {
         return std::vector<double>{static_cast<double>(a.int_field_)};
       }},
      {"uint64_field_", 1, Kind::kUInt64,
       [](const A& a) { return std::vector<double>{U64(a.uint64_field_)}; }},
      {"real_field_", 1, Kind::kReal,
       [](const A& a) { return std::vector<double>{a.real_field_}; }},
      {"real3_field_", 3, Kind::kReal,
       [](const A& a) {
         const auto& v = a.real3_field_;
         return std::vector<double>{v[0], v[1], v[2]};
       }},
      {"real4_field_", 4, Kind::kReal,
       [](const A& a) {
         const auto& v = a.real4_field_;
         return std::vector<double>{v[0], v[1], v[2], v[3]};
       }},
      {"uid_field_", 1, Kind::kUInt64,
       [](const A& a) { return std::vector<double>{U64(a.uid_field_)}; }},
      {"pointer_field_", 1, Kind::kUInt64,
       [](const A& a) {
         return std::vector<double>{U64(a.pointer_field_.GetUidAsUint64())};
       }},
  };
  return expected;
}

// -----------------------------------------------------------------------------
/// Reads the agents of one type that the export wrote for one time step.
/// This is the only function that knows the file format.
vtkSmartPointer<vtkUnstructuredGrid> ReadExportedAgents(
    const std::string& output_dir, const std::string& type_name) {
  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(output_dir)) {
    auto name = entry.path().filename().string();
    if (name.rfind(type_name + "-", 0) == 0 &&
        entry.path().extension() == ".pvtu") {
      files.push_back(entry.path().string());
    }
  }
  EXPECT_EQ(1u, files.size())
      << "expected one " << type_name << "-<step>.pvtu file in " << output_dir;
  if (files.size() != 1) {
    return nullptr;
  }
  vtkNew<vtkXMLPUnstructuredGridReader> reader;
  reader->SetFileName(files[0].c_str());
  reader->Update();
  vtkSmartPointer<vtkUnstructuredGrid> grid = reader->GetOutput();
  return grid;
}

// -----------------------------------------------------------------------------
/// Exports `num_agents` agents with distinct values in every field, using the
/// given mapped data array mode, and checks the files against the agents.
void CheckExport(Param::MappedDataArrayMode mode, uint64_t num_agents) {
  const std::string type_name = "ExportContractAgent";
  auto set_param = [&](Param* param) {
    param->remove_output_dir_contents = true;
    param->export_visualization = true;
    param->insitu_visualization = false;
    param->visualization_export_generate_pvsm = false;
    param->mapped_data_array_mode = mode;
    // Listed explicitly; position_ and diameter_ are required by the agent
    // and exported whether listed or not.
    param->visualize_agents.insert(
        {type_name,
         {"uid_", "int_field_", "uint64_field_", "real_field_", "real3_field_",
          "real4_field_", "uid_field_", "pointer_field_"}});
  };
  Simulation simulation(
      Concat("ExportContractTest_", static_cast<int>(mode), "_", num_agents),
      set_param);
  auto* rm = simulation.GetResourceManager();

  std::vector<ExportContractAgent*> agents;
  for (uint64_t i = 0; i < num_agents; ++i) {
    auto* a = new ExportContractAgent();
    auto d = static_cast<real_t>(i);
    a->SetPosition({d, 2 * d, 3 * d});
    a->SetDiameter(10 + d);
    a->int_field_ = -1000 + static_cast<int>(i);
    a->uint64_field_ = (uint64_t{1} << 40) + i;
    a->real_field_ = 0.5 + d;
    a->real3_field_ = {d, d + 0.25, d + 0.5};
    a->real4_field_ = {d, -d, 2 * d, 0.125};
    a->uid_field_ = AgentUid(static_cast<AgentUid::Index_t>(7 * i + 3), 2);
    rm->AddAgent(a);
    agents.push_back(a);
  }
  // Each agent points to the next one, so that every pointer is valid.
  for (uint64_t i = 0; i < num_agents; ++i) {
    agents[i]->pointer_field_ =
        AgentPointer<Cell>(agents[(i + 1) % num_agents]->GetUid());
  }

  // Export without running a step, which would move the agents.
  (*simulation.GetScheduler()->GetOps("visualize")[0])();

  auto grid = ReadExportedAgents(simulation.GetOutputDir(), type_name);
  ASSERT_NE(nullptr, grid.GetPointer());
  ASSERT_EQ(static_cast<vtkIdType>(num_agents), grid->GetNumberOfPoints());

  // Exactly the expected arrays, and nothing else.
  auto* point_data = grid->GetPointData();
  std::set<std::string> found;
  for (int i = 0; i < point_data->GetNumberOfArrays(); ++i) {
    const char* name = point_data->GetArrayName(i);
    found.insert(name != nullptr ? name : "<unnamed>");
  }
  std::set<std::string> wanted;
  for (const auto& e : ExpectedArrays()) {
    wanted.insert(e.name);
  }
  EXPECT_EQ(wanted, found);
  // position_ becomes the points, not an array.
  EXPECT_EQ(0u, found.count("position_"));

  for (const auto& e : ExpectedArrays()) {
    auto* array = point_data->GetArray(e.name.c_str());
    ASSERT_NE(nullptr, array) << e.name;
    EXPECT_EQ(e.components, array->GetNumberOfComponents()) << e.name;
    EXPECT_TRUE(HasKind(array, e.kind))
        << e.name << " has VTK type " << array->GetDataTypeAsString();
  }

  // Rows are matched to agents by uid, since the export does not promise an
  // order across its pieces.
  std::map<uint64_t, const ExportContractAgent*> by_uid;
  for (auto* a : agents) {
    by_uid[static_cast<uint64_t>(a->GetUid())] = a;
  }
  auto* uids = point_data->GetArray("uid_");
  auto* points = grid->GetPoints();
  std::set<uint64_t> seen;
  for (vtkIdType row = 0; row < grid->GetNumberOfPoints(); ++row) {
    auto uid = static_cast<uint64_t>(uids->GetComponent(row, 0));
    auto it = by_uid.find(uid);
    ASSERT_NE(by_uid.end(), it) << "row " << row << " has unknown uid " << uid;
    EXPECT_TRUE(seen.insert(uid).second) << "uid " << uid << " exported twice";
    const auto& agent = *it->second;

    double point[3];
    points->GetPoint(row, point);
    const auto& position = agent.GetPosition();
    for (int c = 0; c < 3; ++c) {
      EXPECT_EQ(position[c], point[c]) << "position, uid " << uid;
    }
    for (const auto& e : ExpectedArrays()) {
      auto* array = point_data->GetArray(e.name.c_str());
      auto values = e.values(agent);
      for (int c = 0; c < e.components; ++c) {
        EXPECT_EQ(values[c], array->GetComponent(row, c))
            << e.name << "[" << c << "], uid " << uid;
      }
    }
  }
}

// -----------------------------------------------------------------------------
uint64_t SeveralPerThread() {
  return 2 * ThreadInfo::GetInstance()->GetMaxThreads() + 3;
}

TEST(ParaviewExportContractTest, ZeroCopy) {
  CheckExport(Param::MappedDataArrayMode::kZeroCopy, SeveralPerThread());
}

TEST(ParaviewExportContractTest, Cache) {
  CheckExport(Param::MappedDataArrayMode::kCache, SeveralPerThread());
}

TEST(ParaviewExportContractTest, Copy) {
  CheckExport(Param::MappedDataArrayMode::kCopy, SeveralPerThread());
}

// Fewer agents than threads, so that some of the export's pieces are empty.
TEST(ParaviewExportContractTest, FewerAgentsThanThreads) {
  CheckExport(Param::MappedDataArrayMode::kZeroCopy, 1);
}

// A data member of a type the export cannot represent stops the simulation
// with a message that names the member, instead of exporting something wrong.
TEST(ParaviewExportContractDeathTest, UnsupportedFieldTypeIsReported) {
  ASSERT_DEATH(
      {
        auto set_param = [](Param* param) {
          param->remove_output_dir_contents = true;
          param->export_visualization = true;
          param->insitu_visualization = false;
          param->visualization_export_generate_pvsm = false;
          param->visualize_agents.insert(
              {"UnsupportedFieldAgent", {"bool_field_"}});
        };
        Simulation simulation("ExportContractTest_Unsupported", set_param);
        simulation.GetResourceManager()->AddAgent(new UnsupportedFieldAgent());
        (*simulation.GetScheduler()->GetOps("visualize")[0])();
      },
      "bool_field_.*cannot be visualized");
}

}  // namespace export_contract_test_internal
}  // namespace bdm

#endif  // USE_PARAVIEW

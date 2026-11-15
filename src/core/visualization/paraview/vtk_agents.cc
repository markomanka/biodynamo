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

#include "core/visualization/paraview/vtk_agents.h"
// std
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
// ParaView
#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
// ROOT
#include <TClass.h>
#include <TClassTable.h>
#include <TDataMember.h>
// BioDynaMo
#include "core/agent/agent.h"
#include "core/agent/agent_pointer.h"
#include "core/agent/agent_uid.h"
#include "core/container/math_array.h"
#include "core/param/param.h"
#include "core/real_t.h"
#include "core/shape.h"
#include "core/simulation.h"
#include "core/util/jit.h"
#include "core/util/log.h"
#include "core/visualization/paraview/mapped_data_array.h"
#include "core/visualization/paraview/parallel_vtu_writer.h"

#include "core/agent/cell.h"

namespace bdm {

// -----------------------------------------------------------------------------
// The arrays that expose agent data members to VTK are created here from code
// compiled into this library. They used to be created by code that ROOT's
// interpreter compiled for every simulation, because the types of the data
// members are known only at run time; that fails on macOS with an SDK newer
// than 14 (conda-forge/root-feedstock#396). The set of supported types is
// small and fixed, so one array type per kind is instantiated below and chosen
// by the data member's type name. Names, types and offsets still come from
// ROOT's dictionaries.
namespace {

/// What the export can represent: one of three numeric types, an agent uid,
/// or an agent pointer (exported as the uid it points to), and MathArrays of
/// the numeric types.
struct FieldType {
  enum Kind { kUnsupported, kReal, kInt, kUInt64, kAgentUid, kAgentPointer };
  Kind kind = kUnsupported;
  bool is_array = false;
  uint64_t components = 1;
};

/// "bdm::MathArray<double, 3>" -> "MathArray<double,3>"
std::string Normalize(const std::string& type_name) {
  std::string result;
  for (char c : type_name) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      result += c;
    }
  }
  for (const std::string prefix : {"bdm::", "std::"}) {
    for (auto pos = result.find(prefix); pos != std::string::npos;
         pos = result.find(prefix)) {
      result.erase(pos, prefix.size());
    }
  }
  return result;
}

FieldType::Kind ScalarKind(const std::string& name) {
  static const std::set<std::string> real_names = {
      "real_t", sizeof(real_t) == sizeof(double) ? "double" : "float",
      sizeof(real_t) == sizeof(double) ? "Double_t" : "Float_t"};
  static const std::set<std::string> int_names = {"int", "Int_t", "int32_t"};
  // Every spelling of a 64 bit unsigned integer on the supported platforms.
  static const std::set<std::string> uint64_names = {"uint64_t",
                                                     "unsignedlong",
                                                     "unsignedlonglong",
                                                     "unsignedlongint",
                                                     "unsignedlonglongint",
                                                     "longunsignedint",
                                                     "longlongunsignedint",
                                                     "ULong64_t",
                                                     "ULong_t",
                                                     "size_t"};
  if (real_names.count(name) != 0) {
    return FieldType::kReal;
  } else if (int_names.count(name) != 0) {
    return FieldType::kInt;
  } else if (uint64_names.count(name) != 0) {
    return FieldType::kUInt64;
  }
  return FieldType::kUnsupported;
}

FieldType ParseFieldType(const std::string& type_name) {
  FieldType type;
  auto name = Normalize(type_name);
  const std::string array_prefix = "MathArray<";
  if (name == "AgentUid") {
    type.kind = FieldType::kAgentUid;
  } else if (name.rfind("AgentPointer<", 0) == 0) {
    type.kind = FieldType::kAgentPointer;
  } else if (name.rfind(array_prefix, 0) == 0 && name.back() == '>') {
    // MathArray<element,N>
    auto comma = name.find_last_of(',');
    if (comma != std::string::npos && comma > array_prefix.size()) {
      auto element =
          name.substr(array_prefix.size(), comma - array_prefix.size());
      auto size = name.substr(comma + 1, name.size() - comma - 2);
      char* end = nullptr;
      auto components = std::strtoull(size.c_str(), &end, 10);
      // The size may carry an integer suffix, as in "3ul".
      bool only_suffix = true;
      for (; end != nullptr && *end != 0; ++end) {
        only_suffix = only_suffix && (*end == 'u' || *end == 'l' ||
                                      *end == 'U' || *end == 'L');
      }
      if (components > 0 && only_suffix) {
        type.kind = ScalarKind(element);
        type.is_array = true;
        type.components = components;
      }
    }
  } else if (name == "Real3" || name == "Real4") {
    // The aliases, in case a dictionary reports the name as written.
    type.kind = FieldType::kReal;
    type.is_array = true;
    type.components = name == "Real3" ? 3 : 4;
  } else {
    type.kind = ScalarKind(name);
  }
  return type;
}

/// The array for a data member of type `TMember`, read as `TScalar` values.
/// Offsets are taken from the start of the agent, see CheckDeclaringClass.
template <typename TScalar, typename TMember>
vtkSmartPointer<vtkDataArray> NewMappedDataArray(const std::string& name,
                                                 uint64_t components,
                                                 uint64_t offset) {
  vtkNew<MappedDataArray<TScalar, Agent, TMember>> array;
  auto mode = Simulation::GetActive()->GetParam()->mapped_data_array_mode;
  array->Initialize(mode, name, components, offset);
  return vtkSmartPointer<vtkDataArray>(array.GetPointer());
}

/// MathArray<T, N> stores N values of T and nothing else, and data() returns
/// the address of the first, so every N is read through MathArray<T, 1>; the
/// number of components is passed separately. Likewise AgentPointer<T> stores
/// the same for every T, and all are read through AgentPointer<Agent>.
vtkSmartPointer<vtkDataArray> NewMappedDataArray(const FieldType& type,
                                                 const std::string& name,
                                                 uint64_t offset) {
  auto n = type.components;
  switch (type.kind) {
    case FieldType::kReal:
      return type.is_array
                 ? NewMappedDataArray<real_t, MathArray<real_t, 1>>(name, n,
                                                                    offset)
                 : NewMappedDataArray<real_t, real_t>(name, n, offset);
    case FieldType::kInt:
      return type.is_array
                 ? NewMappedDataArray<int, MathArray<int, 1>>(name, n, offset)
                 : NewMappedDataArray<int, int>(name, n, offset);
    case FieldType::kUInt64:
      return type.is_array
                 ? NewMappedDataArray<uint64_t, MathArray<uint64_t, 1>>(name, n,
                                                                        offset)
                 : NewMappedDataArray<uint64_t, uint64_t>(name, n, offset);
    case FieldType::kAgentUid:
      return NewMappedDataArray<uint64_t, AgentUid>(name, n, offset);
    case FieldType::kAgentPointer:
      return NewMappedDataArray<uint64_t, AgentPointer<Agent>>(name, n, offset);
    default:
      return nullptr;
  }
}

/// The type of a data member, from the names ROOT's dictionary gives it:
/// first with typedefs resolved, then as written.
FieldType FindFieldType(TDataMember* tdm) {
  auto type = ParseFieldType(tdm->GetTrueTypeName());
  if (type.kind == FieldType::kUnsupported) {
    type = ParseFieldType(tdm->GetTypeName());
  }
  if (type.kind == FieldType::kUnsupported) {
    Log::Fatal("VtkAgents::VtkAgents", "Data member ", tdm->GetName(), " of ",
               tdm->GetClass()->GetName(), " has type ", tdm->GetTypeName(),
               " (", tdm->GetTrueTypeName(),
               "), which cannot be visualized. Supported are real_t, int, "
               "uint64_t, AgentUid, AgentPointer, and MathArray of real_t, "
               "int or uint64_t (such as Real3).");
  }
  return type;
}

/// Offsets from ROOT are relative to the class that declares the member, and
/// the arrays apply them to the agent's own address. The two agree for a
/// class that derives from Agent, directly or not, as its first base.
void CheckDeclaringClass(TDataMember* tdm) {
  if (!tdm->GetClass()->InheritsFrom("bdm::Agent")) {
    Log::Fatal("VtkAgents::VtkAgents", "Data member ", tdm->GetName(),
               " is declared in ", tdm->GetClass()->GetName(),
               ", which does not derive from bdm::Agent. Only data members "
               "of agent classes can be visualized.");
  }
}

TDataMember* FindDataMember(TClass* tclass, const std::string& name) {
  auto candidates = FindDataMemberSlow(tclass, name);
  if (candidates.size() == 0) {
    Log::Fatal("VtkAgents::VtkAgents", "Could not find data member ", name);
  } else if (candidates.size() > 1) {
    Log::Fatal("VtkAgents::VtkAgents", "Data member name (", name,
               ") is ambiguous");
  }
  return candidates[0];
}

/// Puts the array for one data member into `grid`: positions become the
/// points, everything else an attribute of the points.
void AddDataMember(vtkUnstructuredGrid* grid, TDataMember* tdm,
                   const FieldType& type) {
  std::string name = tdm->GetName();
  auto array = NewMappedDataArray(type, name, tdm->GetOffset());
  bool is_real3 =
      type.kind == FieldType::kReal && type.is_array && type.components == 3;
  if (is_real3 && name == "position_") {
    vtkNew<vtkPoints> points;
    points->SetData(array);
    grid->SetPoints(points.GetPointer());
  } else if (is_real3 && name == "mass_location_") {
    // create points with position {0, 0, 0}
    // BDMGlyph will rotate and translate based on the attribute data
    vtkNew<vtkPoints> points;
    points->SetData(array);
    grid->SetPoints(points.GetPointer());
    grid->GetPointData()->AddArray(array);
  } else {
    grid->GetPointData()->AddArray(array);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
VtkAgents::VtkAgents(const char* type_name,
                     vtkCPDataDescription* data_description) {
  auto* param = Simulation::GetActive()->GetParam();
  auto* tinfo = ThreadInfo::GetInstance();
  if (param->export_visualization) {
    data_.resize(tinfo->GetMaxThreads());
  } else {
    data_.resize(1);
  }
#pragma omp parallel for schedule(static, 1)
  for (uint64_t i = 0; i < data_.size(); ++i) {
    data_[i] = vtkUnstructuredGrid::New();
  }
  name_ = type_name;

  if (!param->export_visualization) {
    data_description->AddInput(type_name);
    data_description->GetInputDescriptionByName(type_name)->SetGrid(data_[0]);
  }

  tclass_ = FindTClass();
  auto* tmp_instance = static_cast<Agent*>(tclass_->New());
  shape_ = tmp_instance->GetShape();
  std::vector<std::string> data_members;
  InitializeDataMembers(tmp_instance, &data_members);

  std::vector<TDataMember*> tdata_members;
  std::vector<FieldType> types;
  for (auto& dm : data_members) {
    auto* tdm = FindDataMember(tclass_, dm);
    CheckDeclaringClass(tdm);
    tdata_members.push_back(tdm);
    types.push_back(FindFieldType(tdm));
  }
  for (uint64_t i = 0; i < data_.size(); ++i) {
    for (uint64_t j = 0; j < tdata_members.size(); ++j) {
      AddDataMember(data_[i], tdata_members[j], types[j]);
    }
  }
}

// -----------------------------------------------------------------------------
VtkAgents::~VtkAgents() {
  name_ = "";
  for (auto& el : data_) {
    el->Delete();
  }
  data_.clear();
}

// -----------------------------------------------------------------------------
vtkUnstructuredGrid* VtkAgents::GetData(uint64_t idx) { return data_[idx]; }

// -----------------------------------------------------------------------------
Shape VtkAgents::GetShape() const { return shape_; }

// -----------------------------------------------------------------------------
TClass* VtkAgents::GetTClass() { return tclass_; }

// -----------------------------------------------------------------------------
void VtkAgents::Update(const std::vector<Agent*>* agents) {
  auto* param = Simulation::GetActive()->GetParam();
  if (param->export_visualization) {
#pragma omp parallel
    {
      auto* tinfo = ThreadInfo::GetInstance();
      auto tid = tinfo->GetMyThreadId();
      auto max_threads = tinfo->GetMaxThreads();

      // use static scheduling for now
      auto correction = agents->size() % max_threads == 0 ? 0 : 1;
      auto chunk = agents->size() / max_threads + correction;
      auto start = tid * chunk;
      auto end = std::min(agents->size(), start + chunk);

      UpdateMappedDataArrays(tid, agents, start, end);
    }
  } else {
    UpdateMappedDataArrays(0, agents, 0, agents->size());
  }
}

// -----------------------------------------------------------------------------
void VtkAgents::WriteToFile(uint64_t step) const {
  auto* sim = Simulation::GetActive();
  auto filename_prefix = Concat(name_, "-", step);

  ParallelVtuWriter writer;
  writer(sim->GetOutputDir(), filename_prefix, data_);
}

// -----------------------------------------------------------------------------
void VtkAgents::UpdateMappedDataArrays(uint64_t tid,
                                       const std::vector<Agent*>* agents,
                                       uint64_t start, uint64_t end) {
  auto* parray = dynamic_cast<MappedDataArrayInterface*>(
      data_[tid]->GetPoints()->GetData());
  parray->Update(agents, start, end);
  auto* point_data = data_[tid]->GetPointData();
  for (int i = 0; i < point_data->GetNumberOfArrays(); i++) {
    auto* array =
        dynamic_cast<MappedDataArrayInterface*>(point_data->GetArray(i));
    array->Update(agents, start, end);
  }
}

// -----------------------------------------------------------------------------
TClass* VtkAgents::FindTClass() {
  const auto& tclass_vector = FindClassSlow(name_);
  if (tclass_vector.size() == 0) {
    Log::Fatal("VtkAgents::VtkAgents",
               Concat("Could not find class: ", name_).c_str());
  } else if (tclass_vector.size() == 0) {
    std::stringstream str;
    for (auto& tc : tclass_vector) {
      str << tc->GetName() << std::endl;
    }
    Log::Fatal("VtkAgents::VtkAgents",
               Concat("Found multiple classes with name : ", name_,
                      ". See list below. Fix this issue by adding a namespace "
                      "modifier.\n'",
                      str.str())
                   .c_str());
  }
  return tclass_vector[0];
}

// -----------------------------------------------------------------------------
void VtkAgents::InitializeDataMembers(
    const Agent* agent, std::vector<std::string>* data_members) const {
  std::set<std::string> dm_set = agent->GetRequiredVisDataMembers();
  auto* param = Simulation::GetActive()->GetParam();
  for (auto& dm : param->visualize_agents.at(name_)) {
    dm_set.insert(dm);
  }
  data_members->resize(dm_set.size());
  std::copy(dm_set.begin(), dm_set.end(), data_members->begin());
}

}  // namespace bdm

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

#ifndef CORE_VISUALIZATION_PARAVIEW_MAPPED_DATA_ARRAY_H_
#define CORE_VISUALIZATION_PARAVIEW_MAPPED_DATA_ARRAY_H_

#include <algorithm>
#include <string>

#include <vtkGenericDataArray.h>
#include <vtkObjectFactory.h>
#include <thread>
#include <vector>

#include "core/agent/agent.h"
#include "core/agent/agent_pointer.h"
#include "core/functor.h"
#include "core/param/param.h"
#include "core/util/thread_info.h"
#include "core/util/type.h"

namespace bdm {

// -----------------------------------------------------------------------------
/// Extract the data member value of an Agent given its concrete type, type
/// of the data member, and data member offset from its concrete pointer.
template <typename TReturn, typename TClass, typename TDataMember>
struct GetDataMemberForVis {
  uint64_t dm_offset_;
  using TempValueType = typename std::remove_pointer<TReturn>::type;
  /// Thread local storage for temporary values.
  mutable std::vector<std::array<TempValueType, 64 / sizeof(TempValueType)>>
      temp_values_;

  GetDataMemberForVis() { temp_values_.resize(256); }

  void Update() {
    // We assume that number of threads won't real_t within one iteration
    temp_values_.resize(
        2 * std::max<unsigned long long>(
                ThreadInfo::GetInstance()->GetMaxUniversalThreadId(), 256ULL));
  }

  enum DataType { kDefault, kArray, kAgentUid, kSoPointer };

  template <typename T>
  static constexpr DataType GetDataType() {
    if (IsArray<T>::value) {
      return DataType::kArray;
    } else if (std::is_same<T, AgentUid>::value) {
      return DataType::kAgentUid;
    } else if (is_agent_ptr<T>::value) {
      return DataType::kSoPointer;
    }
    return DataType::kDefault;
  }

  template <typename TTDataMember = TDataMember>
  typename std::enable_if<GetDataType<TTDataMember>() == DataType::kDefault,
                          TReturn>::type
  operator()(Agent* agent) const {
    auto* casted_agent = static_cast<TClass*>(agent);
    return reinterpret_cast<TDataMember*>(
        reinterpret_cast<char*>(casted_agent) + dm_offset_);
  }

  template <typename TTDataMember = TDataMember>
  typename std::enable_if<GetDataType<TTDataMember>() == DataType::kArray,
                          TReturn>::type
  operator()(Agent* agent) const {
    auto* casted_agent = static_cast<TClass*>(agent);
    auto* data = reinterpret_cast<TDataMember*>(
                     reinterpret_cast<char*>(casted_agent) + dm_offset_)
                     ->data();
    return const_cast<TReturn>(data);
  }

  template <typename TTDataMember = TDataMember>
  typename std::enable_if<GetDataType<TTDataMember>() == DataType::kAgentUid,
                          TReturn>::type
  operator()(Agent* agent) const {
    auto* casted_agent = static_cast<TClass*>(agent);
    auto* data = reinterpret_cast<TDataMember*>(
        reinterpret_cast<char*>(casted_agent) + dm_offset_);
    uint64_t uid = *data;
    auto tid = ThreadInfo::GetInstance()->GetUniversalThreadId();
    assert(temp_values_.size() > tid);
    temp_values_[tid][0] = uid;
    return &temp_values_[tid][0];
  }

  template <typename TTDataMember = TDataMember>
  typename std::enable_if<GetDataType<TTDataMember>() == DataType::kSoPointer,
                          TReturn>::type
  operator()(Agent* agent) const {
    auto* casted_agent = static_cast<TClass*>(agent);
    auto* data = reinterpret_cast<TDataMember*>(
        reinterpret_cast<char*>(casted_agent) + dm_offset_);
    uint64_t uid = data->GetUidAsUint64();
    auto tid = ThreadInfo::GetInstance()->GetUniversalThreadId();
    assert(temp_values_.size() > tid);
    temp_values_[tid][0] = uid;
    return &temp_values_[tid][0];
  }
};

// -----------------------------------------------------------------------------
struct MappedDataArrayInterface {
  MappedDataArrayInterface() = default;
  virtual ~MappedDataArrayInterface() = default;
  virtual void Update(const std::vector<Agent*>* agents, uint64_t start,
                      uint64_t end) = 0;
};

// -----------------------------------------------------------------------------
/// Read-only VTK array whose values are one data member of each agent.
///
/// Built on vtkGenericDataArray (VTK 7 and later; vtkMappedDataArray was
/// removed in VTK 9.4): VTK derives the full vtkDataArray interface from the
/// accessors below. As with vtkMappedDataArray, NewInstance() returns an
/// ordinary writable array of the same value type.
///
/// Modes (Param::MappedDataArrayMode): kZeroCopy reads the agent on every
/// access, kCopy copies all values in Update(), kCache copies a value on its
/// first access after each Update().
template <typename TScalar, typename TClass, typename TDataMember>
class MappedDataArray
    : public vtkGenericDataArray<MappedDataArray<TScalar, TClass, TDataMember>,
                                 TScalar>,
      public MappedDataArrayInterface {
  using GenericBase =
      vtkGenericDataArray<MappedDataArray<TScalar, TClass, TDataMember>,
                          TScalar>;

 public:
  vtkAbstractTemplateTypeMacro(MappedDataArray, GenericBase);
  vtkAOSArrayNewInstanceMacro(MappedDataArray);

 public:
  using ValueType = typename GenericBase::ValueType;
  static MappedDataArray* New();

  void Initialize(Param::MappedDataArrayMode mode, const std::string& name,
                  uint64_t num_components, uint64_t dm_offset);
  void Update(const std::vector<Agent*>* agents, uint64_t start,
              uint64_t end) final;

  // Accessors required by vtkGenericDataArray.
  ValueType GetValue(vtkIdType value_idx) const;
  void GetTypedTuple(vtkIdType tuple_idx, ValueType* tuple) const;
  ValueType GetTypedComponent(vtkIdType tuple_idx, int comp) const;
  // The array is read only: these print a warning and change nothing.
  void SetValue(vtkIdType value_idx, ValueType value);
  void SetTypedTuple(vtkIdType tuple_idx, const ValueType* tuple);
  void SetTypedComponent(vtkIdType tuple_idx, int comp, ValueType value);

 protected:
  MappedDataArray() = default;
  ~MappedDataArray() override = default;

  // The agents own the storage: allocation succeeds only within the size set
  // by Update().
  bool AllocateTuples(vtkIdType num_tuples);
  bool ReallocateTuples(vtkIdType num_tuples);

  friend class vtkGenericDataArray<
      MappedDataArray<TScalar, TClass, TDataMember>, TScalar>;

  /// Access agent data member functor.
  GetDataMemberForVis<TScalar*, TClass, TDataMember> get_dm_;
  const std::vector<Agent*>* agents_ = nullptr;
  uint64_t start_ = 0;
  uint64_t end_ = 0;
  Param::MappedDataArrayMode mode_ = Param::MappedDataArrayMode::kZeroCopy;
  /// Incremented by each Update(); a cached value is valid while the value
  /// stored for it in is_matching_ equals match_value_.
  mutable uint64_t match_value_ = 0;
  mutable std::vector<uint64_t> is_matching_;
  /// Values copied from the agents in modes kCopy and kCache.
  mutable std::vector<TScalar> data_;

 private:
  MappedDataArray(const MappedDataArray&) = delete;
  void operator=(const MappedDataArray&) = delete;
};

// -----------------------------------------------------------------------------
// Implementation
template <typename TScalar, typename TClass, typename TDataMember>
MappedDataArray<TScalar, TClass, TDataMember>*
MappedDataArray<TScalar, TClass, TDataMember>::New() {
  VTK_STANDARD_NEW_BODY(MappedDataArray);
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::Initialize(
    Param::MappedDataArrayMode mode, const std::string& name,
    uint64_t num_components, uint64_t dm_offset) {
  get_dm_.dm_offset_ = dm_offset;
  mode_ = mode;
  this->NumberOfComponents = static_cast<int>(num_components);
  this->SetName(name.c_str());
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::Update(
    const std::vector<Agent*>* agents, uint64_t start, uint64_t end) {
  agents_ = agents;
  start_ = start;
  end_ = end;
  get_dm_.Update();
  const auto size = static_cast<vtkIdType>(this->NumberOfComponents) *
                    static_cast<vtkIdType>(end > start ? end - start : 0);
  this->Size = size;
  this->MaxId = size - 1;
  this->Modified();
  if (size == 0 || mode_ == Param::MappedDataArrayMode::kZeroCopy) {
    return;
  }
  const auto n = static_cast<size_t>(size);
  if (data_.size() < n) {
    data_.resize(n);
  }
  if (mode_ == Param::MappedDataArrayMode::kCopy) {
    size_t counter = 0;
    for (uint64_t i = start; i < end; ++i) {
      auto* data = get_dm_((*agents_)[i]);
      for (int c = 0; c < this->NumberOfComponents; ++c) {
        data_[counter++] = data[c];
      }
    }
  } else {
    // kCache: entries written before this Update() are stale from now on.
    if (is_matching_.size() < n) {
      is_matching_.resize(n, match_value_);
    }
    ++match_value_;
  }
}

template <typename TScalar, typename TClass, typename TDataMember>
typename MappedDataArray<TScalar, TClass, TDataMember>::ValueType
MappedDataArray<TScalar, TClass, TDataMember>::GetTypedComponent(
    vtkIdType tuple_idx, int comp) const {
  const auto idx =
      static_cast<size_t>(tuple_idx * this->NumberOfComponents + comp);
  switch (mode_) {
    case Param::MappedDataArrayMode::kCopy:
      return data_[idx];
    case Param::MappedDataArrayMode::kCache:
      if (is_matching_[idx] != match_value_) {
        data_[idx] = get_dm_((*agents_)[start_ + tuple_idx])[comp];
        is_matching_[idx] = match_value_;
      }
      return data_[idx];
    case Param::MappedDataArrayMode::kZeroCopy:
    default:
      return get_dm_((*agents_)[start_ + tuple_idx])[comp];
  }
}

template <typename TScalar, typename TClass, typename TDataMember>
typename MappedDataArray<TScalar, TClass, TDataMember>::ValueType
MappedDataArray<TScalar, TClass, TDataMember>::GetValue(
    vtkIdType value_idx) const {
  return GetTypedComponent(
      value_idx / this->NumberOfComponents,
      static_cast<int>(value_idx % this->NumberOfComponents));
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::GetTypedTuple(
    vtkIdType tuple_idx, ValueType* tuple) const {
  for (int c = 0; c < this->NumberOfComponents; ++c) {
    tuple[c] = GetTypedComponent(tuple_idx, c);
  }
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::SetValue(vtkIdType,
                                                             ValueType) {
  vtkWarningMacro(<< "MappedDataArray is read only.");
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::SetTypedTuple(
    vtkIdType, const ValueType*) {
  vtkWarningMacro(<< "MappedDataArray is read only.");
}

template <typename TScalar, typename TClass, typename TDataMember>
void MappedDataArray<TScalar, TClass, TDataMember>::SetTypedComponent(
    vtkIdType, int, ValueType) {
  vtkWarningMacro(<< "MappedDataArray is read only.");
}

template <typename TScalar, typename TClass, typename TDataMember>
bool MappedDataArray<TScalar, TClass, TDataMember>::AllocateTuples(
    vtkIdType num_tuples) {
  return ReallocateTuples(num_tuples);
}

template <typename TScalar, typename TClass, typename TDataMember>
bool MappedDataArray<TScalar, TClass, TDataMember>::ReallocateTuples(
    vtkIdType num_tuples) {
  if (num_tuples * this->NumberOfComponents <= this->Size) {
    return true;
  }
  vtkWarningMacro(<< "MappedDataArray is read only and cannot grow.");
  return false;
}

}  // namespace bdm

#endif  // CORE_VISUALIZATION_PARAVIEW_MAPPED_DATA_ARRAY_H_

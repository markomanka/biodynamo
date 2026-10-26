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

#ifndef CORE_UTIL_SHARED_LIBRARY_H_
#define CORE_UTIL_SHARED_LIBRARY_H_

#include <dlfcn.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace bdm {

namespace shared_library_detail {
/// Anchor whose address identifies the binary this header was compiled into.
inline void Anchor() {}
}  // namespace shared_library_detail

/// Platform file name of a shared library, e.g. "Foo" -> "libFoo.so".
inline std::string SharedLibraryFileName(const std::string& name) {
#ifdef __APPLE__
  return "lib" + name + ".dylib";
#else
  return "lib" + name + ".so";
#endif
}

/// Opens shared library `name` and returns the address of `symbol`, or nullptr.
/// Search order: $BDMSYS/lib, the directory of the binary that contains this
/// code (libbiodynamo), then the system search path (LD_LIBRARY_PATH, rpath,
/// ld.so cache). On failure, `error` (if given) describes every attempt.
inline void* FindSymbolInSharedLibrary(const std::string& name,
                                       const char* symbol,
                                       std::string* error = nullptr) {
  const std::string file = SharedLibraryFileName(name);
  std::vector<std::string> candidates;
  if (const char* bdmsys = std::getenv("BDMSYS")) {
    candidates.push_back(std::string(bdmsys) + "/lib/" + file);
  }
  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(&shared_library_detail::Anchor), &info) &&
      info.dli_fname != nullptr) {
    std::string self = info.dli_fname;
    auto slash = self.find_last_of('/');
    if (slash != std::string::npos) {
      candidates.push_back(self.substr(0, slash + 1) + file);
    }
  }
  candidates.push_back(file);
  std::string messages;
  for (const auto& candidate : candidates) {
    void* handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
      const char* reason = dlerror();
      messages += "\n  " + candidate + ": " + (reason ? reason : "not found");
      continue;
    }
    dlerror();  // clear any stale error before dlsym
    void* address = dlsym(handle, symbol);
    if (address != nullptr) {
      return address;
    }
    messages += "\n  " + candidate + ": symbol " + symbol + " not found";
  }
  if (error != nullptr) {
    *error = "could not load " + file + ":" + messages;
  }
  return nullptr;
}

}  // namespace bdm

#endif  // CORE_UTIL_SHARED_LIBRARY_H_

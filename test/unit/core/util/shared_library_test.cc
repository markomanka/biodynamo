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

#include "core/util/shared_library.h"
#include <gtest/gtest.h>

namespace bdm {

TEST(SharedLibraryTest, FileNameFollowsPlatformConvention) {
#ifdef __APPLE__
  EXPECT_EQ("libFoo.dylib", SharedLibraryFileName("Foo"));
#else
  EXPECT_EQ("libFoo.so", SharedLibraryFileName("Foo"));
#endif
}

TEST(SharedLibraryTest, MissingLibraryReportsEveryAttempt) {
  std::string error;
  EXPECT_EQ(nullptr, FindSymbolInSharedLibrary("BdmDoesNotExist",
                                               "NoSuchSymbol", &error));
  EXPECT_NE(std::string::npos,
            error.find(SharedLibraryFileName("BdmDoesNotExist")));
}

}  // namespace bdm

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
// (https://github.com/markomanka/biodynamo), after Aravind Raju's review of
// v1.06 found that logging had no tests of its own.

#include "core/util/log.h"

#include <gtest/gtest.h>

#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace bdm {

class LogTest : public ::testing::Test {
 protected:
  void SetUp() override { level_ = Log::GetLevel(); }
  void TearDown() override { Log::SetLevel(level_); }

  /// What `emit` writes to the error stream.
  template <typename F>
  static std::string Captured(F emit) {
    ::testing::internal::CaptureStderr();
    emit();
    return ::testing::internal::GetCapturedStderr();
  }

 private:
  int level_ = kLogPrint;
};

// The numbers are ROOT's, so that a verbosity chosen for ROOT keeps its
// meaning (and gErrorIgnoreLevel, still set for ROOT's own messages, agrees).
TEST_F(LogTest, LevelsKeepRootsValues) {
  EXPECT_EQ(0, kLogPrint);
  EXPECT_EQ(1000, kLogInfo);
  EXPECT_EQ(2000, kLogWarning);
  EXPECT_EQ(3000, kLogError);
  EXPECT_EQ(4000, kLogBreak);
  EXPECT_EQ(5000, kLogSysError);
  EXPECT_EQ(6000, kLogFatal);
}

TEST_F(LogTest, MessagesKeepRootsFormat) {
  Log::SetLevel(kLogInfo);
  EXPECT_EQ("Info in <Here>: a 3 b\n",
            Captured([] { Log::Info("Here", "a ", 3, " b"); }));
  EXPECT_EQ("Warning in <A::B>: careful\n",
            Captured([] { Log::Warning("A::B", "careful"); }));
  EXPECT_EQ("Error in <X>: 1.5\n", Captured([] { Log::Error("X", 1.5); }));
}

TEST_F(LogTest, MessagesBelowTheLevelAreDropped) {
  Log::SetLevel(kLogWarning);
  EXPECT_EQ(kLogWarning, Log::GetLevel());
  EXPECT_EQ("", Captured([] { Log::Info("L", "dropped"); }));
  EXPECT_EQ("Warning in <L>: kept\n",
            Captured([] { Log::Warning("L", "kept"); }));
  EXPECT_EQ("Error in <L>: kept\n", Captured([] { Log::Error("L", "kept"); }));

  Log::SetLevel(kLogError);
  EXPECT_EQ("", Captured([] { Log::Warning("L", "dropped"); }));
  EXPECT_EQ("Error in <L>: kept\n", Captured([] { Log::Error("L", "kept"); }));

  Log::SetLevel(kLogFatal);
  EXPECT_EQ("", Captured([] { Log::Error("L", "dropped"); }));
}

TEST_F(LogTest, ALevelSetAtTheThresholdLetsThatLevelThrough) {
  Log::SetLevel(kLogInfo);
  EXPECT_EQ("Info in <L>: kept\n", Captured([] { Log::Info("L", "kept"); }));
}

// Many threads logging at once: every line must arrive whole, none lost and
// none interleaved with another.
TEST_F(LogTest, ConcurrentMessagesArriveWhole) {
  Log::SetLevel(kLogInfo);
  const int threads = 8;
  const int lines = 200;
  const std::string out = Captured([&] {
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
      pool.emplace_back([t, lines] {
        for (int i = 0; i < lines; ++i) {
          Log::Warning("Thread", "line ", t, "-", i, " end");
        }
      });
    }
    for (auto& th : pool) {
      th.join();
    }
  });

  std::set<std::string> expected;
  for (int t = 0; t < threads; ++t) {
    for (int i = 0; i < lines; ++i) {
      std::ostringstream line;
      line << "Warning in <Thread>: line " << t << "-" << i << " end";
      expected.insert(line.str());
    }
  }
  std::set<std::string> seen;
  std::istringstream stream(out);
  std::string line;
  int count = 0;
  while (std::getline(stream, line)) {
    ++count;
    EXPECT_EQ(1u, expected.count(line)) << "not a whole message: " << line;
    seen.insert(line);
  }
  EXPECT_EQ(threads * lines, count);
  EXPECT_EQ(expected, seen);
}

TEST_F(LogTest, ConditionPrintsOnlyWhenItHolds) {
  ::testing::internal::CaptureStdout();
  Log::Condition([] { return false; }, "hidden");
  Log::Condition([] { return true; }, "shown ", 42);
  EXPECT_EQ("shown 42\n", ::testing::internal::GetCapturedStdout());
}

}  // namespace bdm

// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <string>
#include <vector>

double calculateMin(const std::vector<double> &samples);
double calculateMax(const std::vector<double> &samples);
double calculateMean(const std::vector<double> &samples);
double calculateMedian(const std::vector<double> &samples);
double calculateStandardDeviation(const std::vector<double> &samples,
                                  double mean);

void printStatistics(const std::string &testCase, const std::string &unit,
                     const std::vector<double> &samples);

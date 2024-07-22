// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "statistics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

double calculateMin(const std::vector<double> &samples) {
    return *std::min_element(samples.begin(), samples.end());
}

double calculateMax(const std::vector<double> &samples) {
    return *std::max_element(samples.begin(), samples.end());
}

double calculateMean(const std::vector<double> &samples) {
    return std::accumulate(samples.begin(), samples.end(), double{0}) /
           samples.size();
}

double calculateMedian(const std::vector<double> &samples) {
    std::vector<double> sortedSamples = samples;
    std::sort(sortedSamples.begin(), sortedSamples.end());
    const auto samplesCount = sortedSamples.size();
    if (samplesCount % 2 == 0) {
        const auto left = sortedSamples[samplesCount / 2 - 1];
        const auto right = sortedSamples[samplesCount / 2];
        return (left + right) / 2;
    } else {
        const auto middle = sortedSamples[samplesCount / 2];
        return middle;
    }
}

double calculateStandardDeviation(const std::vector<double> &samples,
                                  double mean) {
    const auto samplesCount = samples.size();
    double diffSum = 0;
    for (auto i = 0u; i < samplesCount; i++) {
        const auto difference = samples[i] - mean;
        diffSum += difference * difference;
    }
    double stdDev = static_cast<double>(diffSum);
    stdDev /= samplesCount;
    stdDev = std::sqrt(static_cast<double>(stdDev));
    stdDev /= mean;
    return stdDev;
}

void printStatistics(const std::string &testCase, const std::string &unit,
                     const std::vector<double> &samples) {
    const auto min = calculateMin(samples);
    const auto max = calculateMax(samples);
    const auto mean = calculateMean(samples);
    const auto median = calculateMedian(samples);
    const auto stdDev = calculateStandardDeviation(samples, mean);

    std::cout << "TestCase,Mean,Median,StdDev,Min,Max,Type" << std::endl;
    std::cout << testCase << "," << mean << "," << median << "," << stdDev
              << "%," << min << "," << max << ",[CPU]," << unit << std::endl;
}

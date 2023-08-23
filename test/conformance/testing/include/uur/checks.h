// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_CONFORMANCE_INCLUDE_CHECKS_H_INCLUDED
#define UR_CONFORMANCE_INCLUDE_CHECKS_H_INCLUDED

#include <gtest/gtest.h>
#include <ur_api.h>
#include <ur_params.hpp>
#include <uur/utils.h>

namespace uur {

struct Result {
    Result(ur_result_t result) noexcept : value(result) {}

    constexpr bool operator==(const Result &rhs) const noexcept {
        return value == rhs.value;
    }

    ur_result_t value;
};

inline std::ostream &operator<<(std::ostream &out, const Result &result) {
    out << result.value;
    return out;
}

} // namespace uur

#ifndef ASSERT_EQ_RESULT
#define ASSERT_EQ_RESULT(EXPECTED, ACTUAL)                                     \
    ASSERT_EQ(uur::Result(EXPECTED), uur::Result(ACTUAL))
#endif
#ifndef ASSERT_SUCCESS
#define ASSERT_SUCCESS(ACTUAL) ASSERT_EQ_RESULT(UR_RESULT_SUCCESS, ACTUAL)
#endif

#ifndef EXPECT_EQ_RESULT
#define EXPECT_EQ_RESULT(EXPECTED, ACTUAL)                                     \
    EXPECT_EQ(uur::Result(EXPECTED), uur::Result(ACTUAL))
#endif
#ifndef EXPECT_SUCCESS
#define EXPECT_SUCCESS(ACTUAL) EXPECT_EQ_RESULT(UR_RESULT_SUCCESS, ACTUAL)
#endif
#ifndef EXPECT_NO_CRASH
#define EXPECT_NO_CRASH(ACTUAL)                                                \
    EXPECT_EXIT((ACTUAL, exit(0)), ::testing::ExitedWithCode(0), ".*");
#endif

// This is a copy of the TEST_P macro from googletest, instead TestBody is
// defined to call InnerTestBody wrapped with EXPECT_NO_CRASH
#ifndef CONF_TEST_P
#define CONF_TEST_P(test_suite_name, test_name)                                \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                   \
        : public test_suite_name {                                             \
      public:                                                                  \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                \
        void InnerTestBody() override;                                         \
        void TestBody() override;                                              \
                                                                               \
      private:                                                                 \
        static int AddToRegistry() {                                           \
            ::testing::UnitTest::GetInstance()                                 \
                ->parameterized_test_registry()                                \
                .GetTestSuitePatternHolder<test_suite_name>(                   \
                    GTEST_STRINGIFY_(test_suite_name),                         \
                    ::testing::internal::CodeLocation(__FILE__, __LINE__))     \
                ->AddTestPattern(                                              \
                    GTEST_STRINGIFY_(test_suite_name),                         \
                    GTEST_STRINGIFY_(test_name),                               \
                    new ::testing::internal::TestMetaFactory<                  \
                        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)>(), \
                    ::testing::internal::CodeLocation(__FILE__, __LINE__));    \
            return 0;                                                          \
        }                                                                      \
        static int gtest_registering_dummy_ GTEST_ATTRIBUTE_UNUSED_;           \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                     \
        (const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete; \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &operator=(         \
            const GTEST_TEST_CLASS_NAME_(test_suite_name,                      \
                                         test_name) &) = delete; /* NOLINT */  \
    };                                                                         \
    int GTEST_TEST_CLASS_NAME_(test_suite_name,                                \
                               test_name)::gtest_registering_dummy_ =          \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();   \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {      \
        return EXPECT_NO_CRASH(GTEST_TEST_CLASS_NAME_(                         \
            test_suite_name, test_name)::InnerTestBody());                     \
    }                                                                          \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::InnerTestBody()
#endif

inline std::ostream &operator<<(std::ostream &out,
                                const ur_device_handle_t &device) {
    out << uur::GetDeviceName(device);
    return out;
}

#endif // UR_CONFORMANCE_INCLUDE_CHECKS_H_INCLUDED

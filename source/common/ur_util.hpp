/*
 *
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UR_UTIL_H
#define UR_UTIL_H 1

#include <ur_api.h>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
inline int ur_getpid(void) { return static_cast<int>(GetCurrentProcessId()); }
#else

#include <unistd.h>
inline int ur_getpid(void) { return static_cast<int>(getpid()); }
#endif

/* for compatibility with non-clang compilers */
#if defined(__has_feature)
#define CLANG_HAS_FEATURE(x) __has_feature(x)
#else
#define CLANG_HAS_FEATURE(x) 0
#endif

/* define for running with address sanitizer */
#if CLANG_HAS_FEATURE(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define SANITIZER_ADDRESS
#endif

/* define for running with memory sanitizer */
#if CLANG_HAS_FEATURE(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#define SANITIZER_THREAD
#endif

/* define for running with memory sanitizer */
#if CLANG_HAS_FEATURE(memory_sanitizer)
#define SANITIZER_MEMORY
#endif

/* define for running with any sanitizer runtime */
#if defined(SANITIZER_MEMORY) || defined(SANITIZER_ADDRESS) ||                 \
    defined(SANITIZER_THREAD)
#define SANITIZER_ANY
#endif
///////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32)
#define MAKE_LIBRARY_NAME(NAME, VERSION) NAME ".dll"
#else
#define HMODULE void *
#if defined(__APPLE__)
#define MAKE_LIBRARY_NAME(NAME, VERSION) "lib" NAME "." VERSION ".dylib"
#else
#define MAKE_LIBRARY_NAME(NAME, VERSION) "lib" NAME ".so." VERSION
#endif
#endif

inline std::string create_library_path(const char *name, const char *path) {
    std::string library_path;
    if (path && (strcmp("", path) != 0)) {
        library_path.assign(path);
#ifdef _WIN32
        library_path.append("\\");
#else
        library_path.append("/");
#endif
        library_path.append(name);
    } else {
        library_path.assign(name);
    }
    return library_path;
}

//////////////////////////////////////////////////////////////////////////
#if !defined(_WIN32) && (__GNUC__ >= 4)
#define __urdlllocal __attribute__((visibility("hidden")))
#else
#define __urdlllocal
#endif

///////////////////////////////////////////////////////////////////////////////
std::optional<std::string> ur_getenv(const char *name);

inline bool getenv_tobool(const char *name) {
    auto env = ur_getenv(name);
    return env.has_value();
}

static void throw_wrong_format_vec(const char *env_var_name,
                                   std::string env_var_value) {
    std::stringstream ex_ss;
    ex_ss << "Wrong format of the " << env_var_name
          << " environment variable value: '" << env_var_value << "'\n"
          << "Proper format is: "
             "ENV_VAR=\"value_1\",\"value_2\",\"value_3\"";
    throw std::invalid_argument(ex_ss.str());
}

static void throw_wrong_format_map(const char *env_var_name,
                                   std::string env_var_value) {
    std::stringstream ex_ss;
    ex_ss << "Wrong format of the " << env_var_name
          << " environment variable value: '" << env_var_value << "'\n"
          << "Proper format is: "
             "ENV_VAR=\"param_1:value_1,value_2;param_2:value_1\"";
    throw std::invalid_argument(ex_ss.str());
}

/// @brief Get a vector of values from an environment variable \p env_var_name
///        A comma is a delimiter for extracting values from env var string.
///        Colons and semicolons are allowed only inside quotes to align with
///        the similar getenv_to_map() util function and avoid confusion.
///        A vector with a single value is allowed.
///        Env var must consist of strings separated by commas, ie.:
///        ENV_VAR=1,4K,2M
/// @param env_var_name name of an environment variable to be parsed
/// @return std::optional with a possible vector of strings containing parsed values
///         and std::nullopt when the environment variable is not set or is empty
/// @throws std::invalid_argument() when the parsed environment variable has wrong format
inline std::optional<std::vector<std::string>>
getenv_to_vec(const char *env_var_name) {
    char values_delim = ',';

    auto env_var = ur_getenv(env_var_name);
    if (!env_var.has_value()) {
        return std::nullopt;
    }

    auto is_quoted = [](std::string &str) {
        return (str.front() == '\'' && str.back() == '\'') ||
               (str.front() == '"' && str.back() == '"');
    };
    auto has_colon = [](std::string &str) {
        return str.find(':') != std::string::npos;
    };
    auto has_semicolon = [](std::string &str) {
        return str.find(';') != std::string::npos;
    };

    std::vector<std::string> values_vec;
    std::stringstream ss(*env_var);
    std::string value;
    while (std::getline(ss, value, values_delim)) {
        if (value.empty() ||
            (!is_quoted(value) && (has_colon(value) || has_semicolon(value)))) {
            throw_wrong_format_vec(env_var_name, *env_var);
        }

        if (is_quoted(value)) {
            value.erase(value.cbegin());
            value.erase(value.cend() - 1);
        }

        values_vec.push_back(value);
    }

    return values_vec;
}

using EnvVarMap = std::map<std::string, std::vector<std::string>>;

/// @brief Get a map of parameters and their values from an environment variable
///        \p env_var_name
///        Semicolon is a delimiter for extracting key-values pairs from
///        an env var string. Colon is a delimiter for splitting key-values pairs
///        into keys and their values. Comma is a delimiter for values.
///        All special characters in parameter and value strings are allowed except
///        the delimiters.
///        Env vars without parameter names are not allowed, use the getenv_to_vec()
///        util function instead.
///        Keys in a map are parsed parameters and values are vectors of strings
///        containing parameters' values, ie.:
///        ENV_VAR="param_1:value_1,value_2;param_2:value_1"
///        result map:
///             map[param_1] = [value_1, value_2]
///             map[param_2] = [value_1]
/// @param env_var_name name of an environment variable to be parsed
/// @return std::optional with a possible map with parsed parameters as keys and
///         vectors of strings containing parsed values as keys.
///         Otherwise, optional is set to std::nullopt when the environment variable
///         is not set or is empty.
/// @throws std::invalid_argument() when the parsed environment variable has wrong format
inline std::optional<EnvVarMap> getenv_to_map(const char *env_var_name,
                                              bool reject_empty = true) {
    char main_delim = ';';
    char key_value_delim = ':';
    char values_delim = ',';
    EnvVarMap map;

    auto env_var = ur_getenv(env_var_name);
    if (!env_var.has_value()) {
        return std::nullopt;
    }

    auto is_quoted = [](std::string &str) {
        return (str.front() == '\'' && str.back() == '\'') ||
               (str.front() == '"' && str.back() == '"');
    };
    auto has_colon = [](std::string &str) {
        return str.find(':') != std::string::npos;
    };

    std::stringstream ss(*env_var);
    std::string key_value;
    while (std::getline(ss, key_value, main_delim)) {
        std::string key;
        std::string values;
        std::stringstream kv_ss(key_value);

        if (reject_empty && !has_colon(key_value)) {
            throw_wrong_format_map(env_var_name, *env_var);
        }

        std::getline(kv_ss, key, key_value_delim);
        std::getline(kv_ss, values);
        if (key.empty() || (reject_empty && values.empty()) ||
            map.find(key) != map.end()) {
            throw_wrong_format_map(env_var_name, *env_var);
        }

        std::vector<std::string> values_vec;
        std::stringstream values_ss(values);
        std::string value;
        while (std::getline(values_ss, value, values_delim)) {
            if (value.empty() || (has_colon(value) && !is_quoted(value))) {
                throw_wrong_format_map(env_var_name, *env_var);
            }
            if (is_quoted(value)) {
                value.erase(value.cbegin());
                value.pop_back();
            }
            values_vec.push_back(value);
        }
        map[key] = values_vec;
    }
    return map;
}

inline std::size_t combine_hashes(std::size_t seed) { return seed; }

template <typename T, typename... Args>
inline std::size_t combine_hashes(std::size_t seed, const T &v, Args... args) {
    return combine_hashes(seed ^ std::hash<T>{}(v), args...);
}

inline ur_result_t exceptionToResult(std::exception_ptr eptr) {
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
        return UR_RESULT_SUCCESS;
    } catch (std::bad_alloc &) {
        return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (const ur_result_t &e) {
        return e;
    } catch (...) {
        return UR_RESULT_ERROR_UNKNOWN;
    }
}

template <class> inline constexpr bool ur_always_false_t = false;

// TODO: promote all of the below extensions to the Unified Runtime
//       and get rid of these ZER_EXT constants.
const ur_device_info_t UR_EXT_DEVICE_INFO_OPENCL_C_VERSION =
    (ur_device_info_t)0x103D;

const ur_command_t UR_EXT_COMMAND_TYPE_USER =
    (ur_command_t)((uint32_t)UR_COMMAND_FORCE_UINT32 - 1);

/// Program metadata tags recognized by the UR adapters. For kernels the tag
/// must appear after the kernel name.
#define __SYCL_UR_PROGRAM_METADATA_TAG_REQD_WORK_GROUP_SIZE                    \
    "@reqd_work_group_size"
#define __SYCL_UR_PROGRAM_METADATA_GLOBAL_ID_MAPPING "@global_id_mapping"
#define __SYCL_UR_PROGRAM_METADATA_TAG_NEED_FINALIZATION "Requires finalization"

// Terminates the process with a catastrophic error message.
[[noreturn]] inline void die(const char *Message) {
    std::cerr << "die: " << Message << std::endl;
    std::terminate();
}

// A single-threaded app has an opportunity to enable this mode to avoid
// overhead from mutex locking. Default value is 0 which means that single
// thread mode is disabled.
extern const bool SingleThreadMode;

// The wrapper for immutable data.
// The data is initialized only once at first access (via ->) with the
// initialization function provided in Init. All subsequent access to
// the data just returns the already stored data.
//
template <class T> struct ZeCache : private T {
    // The initialization function takes a reference to the data
    // it is going to initialize, since it is private here in
    // order to disallow access other than through "->".
    //
    using InitFunctionType = std::function<void(T &)>;
    InitFunctionType Compute{nullptr};
    std::once_flag Computed;

    ZeCache() : T{} {}

    // Access to the fields of the original T data structure.
    T *operator->() {
        std::call_once(Computed, Compute, static_cast<T &>(*this));
        return this;
    }
};

// Helper for one-liner validation
#define UR_ASSERT(Condition, Error)                                            \
    if (!(Condition))                                                          \
        return Error;

// The getInfo*/ReturnHelper facilities provide shortcut way of
// writing return bytes for the various getInfo APIs.
namespace ur {

[[noreturn]] inline void unreachable() {
#ifdef _MSC_VER
    __assume(0);
#else
    __builtin_unreachable();
#endif
}

// Class which acts like shared_mutex if SingleThreadMode variable is not set.
// If SingleThreadMode variable is set then mutex operations are turned into
// nop.
class SharedMutex {
    std::shared_mutex Mutex;

  public:
    void lock() {
        if (!SingleThreadMode) {
            Mutex.lock();
        }
    }
    bool try_lock() { return SingleThreadMode ? true : Mutex.try_lock(); }
    void unlock() {
        if (!SingleThreadMode) {
            Mutex.unlock();
        }
    }

    void lock_shared() {
        if (!SingleThreadMode) {
            Mutex.lock_shared();
        }
    }
    bool try_lock_shared() {
        return SingleThreadMode ? true : Mutex.try_lock_shared();
    }
    void unlock_shared() {
        if (!SingleThreadMode) {
            Mutex.unlock_shared();
        }
    }
};

// Class which acts like std::mutex if SingleThreadMode variable is not set.
// If SingleThreadMode variable is set then mutex operations are turned into
// nop.
class Mutex {
    std::mutex Mutex;
    friend class Lock;

  public:
    void lock() {
        if (!SingleThreadMode) {
            Mutex.lock();
        }
    }
    bool try_lock() { return SingleThreadMode ? true : Mutex.try_lock(); }
    void unlock() {
        if (!SingleThreadMode) {
            Mutex.unlock();
        }
    }
};

class Lock {
    std::unique_lock<std::mutex> UniqueLock;

  public:
    explicit Lock(Mutex &Mutex) {
        if (!SingleThreadMode) {
            UniqueLock = std::unique_lock<std::mutex>(Mutex.Mutex);
        }
    }
};

/// SpinLock is a synchronization primitive, that uses atomic variable and
/// causes thread trying acquire lock wait in loop while repeatedly check if
/// the lock is available.
///
/// One important feature of this implementation is that std::atomic<bool> can
/// be zero-initialized. This allows SpinLock to have trivial constructor and
/// destructor, which makes it possible to use it in global context (unlike
/// std::mutex, that doesn't provide such guarantees).
class SpinLock {
  public:
    void lock() {
        while (MLock.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
    void unlock() { MLock.clear(std::memory_order_release); }

  private:
    std::atomic_flag MLock = ATOMIC_FLAG_INIT;
};

template <typename T, typename Assign>
ur_result_t getInfoImpl(size_t ParamValueSize, void *ParamValue,
                        size_t *ParamValueSizeRet, T Value, size_t ValueSize,
                        Assign &&AssignFunc) {
    if (!ParamValue && !ParamValueSizeRet) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (ParamValue != nullptr) {

        if (ParamValueSize < ValueSize) {
            return UR_RESULT_ERROR_INVALID_SIZE;
        }

        AssignFunc(ParamValue, Value, ValueSize);
    }

    if (ParamValueSizeRet != nullptr) {
        *ParamValueSizeRet = ValueSize;
    }

    return UR_RESULT_SUCCESS;
}

template <typename T>
ur_result_t getInfo(size_t ParamValueSize, void *ParamValue,
                    size_t *ParamValueSizeRet, T Value) {

    auto assignment = [](void *ParamValue, T Value, size_t ValueSize) {
        std::ignore = ValueSize;
        *static_cast<T *>(ParamValue) = Value;
    };

    return getInfoImpl(ParamValueSize, ParamValue, ParamValueSizeRet, Value,
                       sizeof(T), assignment);
}

template <typename T>
ur_result_t getInfoArray(size_t ArrayLength, size_t ParamValueSize,
                         void *ParamValue, size_t *ParamValueSizeRet,
                         const T *value) {
    return getInfoImpl(ParamValueSize, ParamValue, ParamValueSizeRet, value,
                       ArrayLength * sizeof(T), memcpy);
}

template <typename T, typename RetType>
ur_result_t getInfoArray(size_t ArrayLength, size_t ParamValueSize,
                         void *ParamValue, size_t *ParamValueSizeRet,
                         const T *value) {
    if (ParamValue) {
        memset(ParamValue, 0, ParamValueSize);
        for (uint32_t I = 0; I < ArrayLength; I++) {
            ((RetType *)ParamValue)[I] = (RetType)value[I];
        }
    }
    if (ParamValueSizeRet) {
        *ParamValueSizeRet = ArrayLength * sizeof(RetType);
    }
    return UR_RESULT_SUCCESS;
}

template <>
inline ur_result_t
getInfo<const char *>(size_t ParamValueSize, void *ParamValue,
                      size_t *ParamValueSizeRet, const char *Value) {
    return getInfoArray(strlen(Value) + 1, ParamValueSize, ParamValue,
                        ParamValueSizeRet, Value);
}

class ReturnHelper {
  public:
    ReturnHelper(size_t ParamValueSize, void *ParamValue,
                 size_t *ParamValueSizeRet)
        : ParamValueSize(ParamValueSize), ParamValue(ParamValue),
          ParamValueSizeRet(ParamValueSizeRet) {}

    // A version where in/out info size is represented by a single pointer
    // to a value which is updated on return
    ReturnHelper(size_t *ParamValueSize, void *ParamValue)
        : ParamValueSize(*ParamValueSize), ParamValue(ParamValue),
          ParamValueSizeRet(ParamValueSize) {}

    // Scalar return value
    template <class T> ur_result_t operator()(const T &t) {
        return ur::getInfo(ParamValueSize, ParamValue, ParamValueSizeRet, t);
    }

    // Array return value
    template <class T> ur_result_t operator()(const T *t, size_t s) {
        return ur::getInfoArray(s, ParamValueSize, ParamValue,
                                ParamValueSizeRet, t);
    }

    // Array return value where element type is differrent from T
    template <class RetType, class T>
    ur_result_t operator()(const T *t, size_t s) {
        return ur::getInfoArray<T, RetType>(s, ParamValueSize, ParamValue,
                                            ParamValueSizeRet, t);
    }

  protected:
    size_t ParamValueSize;
    void *ParamValue;
    size_t *ParamValueSizeRet;
};

} // namespace ur

template <class To, class From> To ur_cast(From Value) {
    // TODO: see if more sanity checks are possible.
    assert(sizeof(From) == sizeof(To));
    return (To)(Value);
}

template <> uint32_t inline ur_cast(uint64_t Value) {
    // Cast value and check that we don't lose any information.
    uint32_t CastedValue = (uint32_t)(Value);
    assert((uint64_t)CastedValue == Value);
    return CastedValue;
}

// Controls tracing UR calls from within the UR itself.
extern bool PrintTrace;

// Apparatus for maintaining immutable cache of platforms.
//
// Note we only create a simple pointer variables such that C++ RT won't
// deallocate them automatically at the end of the main program.
// The heap memory allocated for these global variables reclaimed only at
// explicit tear-down.
extern std::vector<ur_platform_handle_t> *URPlatformsCache;
extern ur::SpinLock *URPlatformsCacheMutex;
extern bool URPlatformCachePopulated;

#endif /* UR_UTIL_H */

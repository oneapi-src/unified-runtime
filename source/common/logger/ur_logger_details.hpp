// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_LOGGER_DETAILS_HPP
#define UR_LOGGER_DETAILS_HPP 1

#include "ur_level.hpp"
#include "ur_sinks.hpp"

namespace logger {

struct LegacyMessage {
  LegacyMessage(const char *p) : message(p) {};
  const char *message;
};

class Logger {
public:
  Logger(std::unique_ptr<Sink> sink)
      : level(Level::QUIET), sink(std::move(sink)) {}

  Logger(Level level, std::unique_ptr<Sink> sink)
      : level(level), sink(std::move(sink)) {}

  void setLevel(Level level) { this->level = level; }

  Level getLevel() { return this->level; }

  void setFlushLevel(Level level) {
    if (sink) {
      this->sink->setFlushLevel(level);
    }
  }

  template <typename... Args> void debug(Args &&...args) {
    log(Level::DEBUG, std::forward<Args>(args)...);
  }

  template <typename... Args> void info(Args &&...args) {
    log(Level::INFO, std::forward<Args>(args)...);
  }

  template <typename... Args> void warning(Args &&...args) {
    log(Level::WARN, std::forward<Args>(args)...);
  }

  template <typename... Args> void warn(Args &&...args) {
    warning(std::forward<Args>(args)...);
  }

  template <typename... Args> void error(Args &&...args) {
    log(Level::ERR, std::forward<Args>(args)...);
  }

  template <typename... Args> void always(Args &&...args) {
    if (sink) {
      sink->log(Level::QUIET, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void debug(const LegacyMessage &p, Args &&...args) {
    log(p, Level::DEBUG, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(const LegacyMessage &p, Args &&...args) {
    log(p, Level::INFO, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warning(const LegacyMessage &p, Args &&...args) {
    log(p, Level::WARN, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(const LegacyMessage &p, Args &&...args) {
    log(p, Level::ERR, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void log(Level level, const char *filename, const char *lineno,
           const char *format, Args &&...args) {
    log(LegacyMessage(format), level, filename, lineno, format,
        std::forward<Args>(args)...);
  }

  template <typename... Args>
  void log(const LegacyMessage &p, Level level, const char *filename,
           const char *lineno, const char *format, Args &&...args) {
    if (!sink) {
      return;
    }

    if (isLegacySink) {
      sink->log(level, filename, lineno, p.message,
                std::forward<Args>(args)...);
      return;
    }
    if (level < this->level) {
      return;
    }

    sink->log(level, filename, lineno, format, std::forward<Args>(args)...);
  }

  void setLegacySink(std::unique_ptr<Sink> legacySink) {
    this->isLegacySink = true;
    this->sink = std::move(legacySink);
  }

private:
  Level level;
  std::unique_ptr<Sink> sink;
  bool isLegacySink = false;
};

} // namespace logger

#ifdef SRC_PATH_SIZE
#define SHORT_FILE ((__FILE__) + (SRC_PATH_SIZE))
#else
#define SHORT_FILE __FILE__
#endif

#define UR_STRIMPL(x) #x
#define UR_STR(x) UR_STRIMPL(x)

#define URLOG_(logger_instance, level, ...)                                    \
  {                                                                            \
    (logger_instance)                                                          \
        .log(logger::Level::level, SHORT_FILE, UR_STR(__LINE__), __VA_ARGS__); \
  }

#define URLOG_ALWAYS_(logger_instance, ...)                                    \
  { (logger_instance).always(SHORT_FILE, UR_STR(__LINE__), __VA_ARGS__); }

#endif /* UR_LOGGER_DETAILS_HPP */

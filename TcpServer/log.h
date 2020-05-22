// Copyright 2019 zhangke
#ifndef TCPSERVER_LOG_H_
#define TCPSERVER_LOG_H_

#include <pthread.h>
#include <sys/time.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

class Logger {
 public:
  enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
  };
  explicit Logger(const std::string &logFileName,
                  LogLevel defaultLevel = LOG_DEBUG)
      : logStream(logFileName, std::ostream::app) {
    setNewLevel(defaultLevel);
  }
  void reOpen(const std::string &newLogFileName) {
    if (logStream.is_open()) {
      logStream.close();
      logStream.clear();
    }
    logStream.open(newLogFileName);
  }
  Logger(const Logger &) = delete;             // 禁止拷贝构造函数
  Logger &operator=(const Logger &) = delete;  // 禁止拷贝赋值运算符
  LogLevel getLevel() { return displayLevel; }
  void setLevel(LogLevel newLevel) { setNewLevel(newLevel); }
  void printLog(LogLevel level, const std::string &msg, const char *filename,
                int linenumber);

 private:
  std::ofstream logStream;
  LogLevel displayLevel;
  void setNewLevel(LogLevel level) {
    assert(level == LOG_DEBUG || level == LOG_INFO || level == LOG_WARN ||
           level == LOG_ERROR);
    displayLevel = level;
  }
};

// use marco to avoid always input __FILE__, __LINE__
#define Log(logger, level, message)                            \
  \
do {                                                           \
    (logger).printLog((level), (message), __FILE__, __LINE__); \
  \
}                                                         \
  while (0)

extern Logger logger;

#endif  // TCPSERVER_LOG_H_

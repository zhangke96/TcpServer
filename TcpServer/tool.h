// Copyright 2019 zhangke
#ifndef TCPSERVER_TOOL_H_
#define TCPSERVER_TOOL_H_

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include "TcpServer/log.h"

std::string createNotFound();
std::string createNotFound(const std::string &);
std::string createOk();
std::string createOk(const std::string &);
ssize_t readn(int fd, char *buf, size_t nbytes);         // copy from apue
ssize_t writen(int fd, const char *buf, size_t nbytes);  // copy from apue
void ignoreSignal(); /* ignore signal SIGPIPE */

class CharStream {
 public:
  virtual ssize_t read(char *, size_t size) = 0;
  virtual ~CharStream() {}
  virtual bool end() const = 0;
};

#endif  // TCPSERVER_TOOL_H_

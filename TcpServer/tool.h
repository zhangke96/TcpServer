
#ifndef ZK_TOOL
#define ZK_TOOL
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "log.h"
 
std::string createNotFound();
std::string createOk();
std::string createOk(const std::string &);
ssize_t readn(int fd, char *buf, size_t nbytes);  // copy from apue
ssize_t writen(int fd, const char *buf, size_t nbytes); // copy from apue
void ignoreSignal();  /* ignore signal SIGPIPE */

#endif

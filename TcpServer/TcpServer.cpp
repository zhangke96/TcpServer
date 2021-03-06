// Copyright 2019 zhangke
#include "TcpServer/TcpServer.h"

TcpServer::~TcpServer() {}

bool operator<(const TcpConnection &lhs, const TcpConnection &rhs) {
  return lhs.fd < rhs.fd;
}

WriteMeta string2WriteMeta(const std::string &str) {
  char *buf = new char[str.length()];
  assert(buf);
  memcpy(buf, str.c_str(), str.length());
  return WriteMeta(std::shared_ptr<char>(buf, [](char *p) { delete[] p; }),
                   str.length());
}

bool operator==(const TcpConnection &lhs, const TcpConnection &rhs) {
  return lhs.fd == rhs.fd &&
         strncmp((const char *)&(lhs.address), (const char *)&(rhs.address),
                 sizeof(struct sockaddr_in)) == 0;
}

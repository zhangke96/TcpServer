// Copyright 2019 zhangke
#ifdef TCP_SERVER_TEST

#include <unistd.h>
#include <iostream>
#include <list>
#include <thread>
#include <utility>
#include <vector>
#include "TcpServer/TcpServer.h"
#include "TcpServer/tool.h"

std::vector<std::pair<int, EpollChangeOperation>> changes;

// typedef OnConnectOperation(*OnConnectHandle_t)(const TcpConnection*, void *);
// typedef void(*OnReadHandle_t)(const TcpConnection*, const char *, size_t,
// void *);
// typedef void(*OnPeerShutdownHandle_t)(const TcpConnection *, void *);
// typedef void(*OnCanWriteHandle_t)(const TcpConnection*, void *);

void deleter(char* p) { delete[] p; }
int main() {
  TcpServer tcpServer;
  std::cout << "setAddressPort result: " << std::boolalpha
            << tcpServer.setAddressPort("0.0.0.0", 1234) << std::endl;
  std::cout << "getAddress: " << tcpServer.getAddress() << std::endl;
  std::cout << "getPort: " << tcpServer.getPort() << std::endl;
  bool startResult = false;
  tcpServer.onConnect(
      [&tcpServer](const TcpConnection* connection, void* data)
          -> OnConnectOperation { return OnConnectOperation::ADD_READ; });
  tcpServer.onNewData([&tcpServer](const TcpConnection* connection,
                                   const char* data, size_t size, void*) {
    if (size == 4 && (strncmp(data, "ping", 4) == 0)) {
      char* respsond = new char[4];
      memcpy(respsond, "pong", 4);
      tcpServer.notifyCanWrite(
          connection->fd,
          WriteMeta(std::shared_ptr<char>(respsond, deleter), 4));
    } else {
      tcpServer.notifyChangeEpoll(
          {{connection->fd, EpollChangeOperation::CLOSE_IT}});
    }
  });
  tcpServer.onShutdown([](const TcpConnection*, void*) {});
  tcpServer.onCanWrite([](const TcpConnection*, void*) {});
  startResult = tcpServer.start();
  if (!startResult) {
    std::cout << "getError: " << tcpServer.getError() << std::endl;
  } else {
    std::cout << "Start ok" << std::endl;
  }
  std::thread t(tcpServer.runServer(false));
  t.join();
}

#endif  // TCP_SERVER_TEST

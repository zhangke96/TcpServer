# TcpServer

TcpServer only work with [epoll](https://en.wikipedia.org/wiki/Epoll).

### Usage:
```cpp
OnConnectOperation connection(const Connection *connection, void *data);
void onReadHandler(const Connection *, const char *, size_t , void *);
void onPeerShutdownHandler(const Connection *, void *);

TcpServer server;
server.setAddressPort("0.0.0.0", 80);
server.onConnect(connectHandler);	// TcpServer will callback when new connection
server.onNewData(onReadHandler);	// TcpServer will callbakck when new data read
server.onShutdown(onPeerShutdownHandler);	// TcpServer will callback when client shutdown the connection

std::thread t(server.runServer(false));	// TcpServer will run and create a thread
t.join();

std::vector<std::pair<int, EpollChangeOperation>> change {};
server.notifyChangeEpoll(change);	// User can notify server to add write or close connection

WriteMeta toWrite(buffer, length);
server.notifyCanWrite(fd, toWrite);	// User can notify server to write content to client
```
[Sample](/TcpServer/TcpServer.test.cpp) can run

### Demo:
A [Http Server](https://github.com/zhangke96/ModernHttpServer) use this TcpServer.

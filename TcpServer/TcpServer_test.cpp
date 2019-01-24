#ifdef TCP_SERVER_TEST

#include "TcpServer.h"
#include "tool.h"
#include <iostream>
#include <list>
#include <unistd.h>
using namespace std;

OnConnectOperation connectHandler(const Connection * connection, void *data);	// TcpServer will callback this function when a new connection
void onReadHandler(const Connection *, const char *, size_t, void *);			// TcpServer will callback when receive new data from client
void onPeerShutdownHandler(const Connection *, void *);
std::vector < std::pair<int, EpollChangeOperation>> changes;
TcpServer tcpServer;

int main()
{
	cout << "setAddressPort result: " << boolalpha
		<< tcpServer.setAddressPort("0.0.0.0", 1234) << endl;
	cout << "getAddress: " << tcpServer.getAddress() << endl;
	cout << "getPort: " << tcpServer.getPort() << endl;
	bool startResult = false;
	tcpServer.onConnect(connectHandler);
	tcpServer.onNewData(onReadHandler);
	tcpServer.onShutdown(onPeerShutdownHandler);
	startResult = tcpServer.start();
	if (!startResult)
	{
		cout << "getError: " << tcpServer.getError() << endl;
	}
	else
	{
		cout << "Start ok" << endl;
	}
	std::thread t(tcpServer.runServer(false));
	t.join();
}

OnConnectOperation connectHandler(const Connection * connection, void *)
{
	return OnConnectOperation::ADD_READ;
}
void onReadHandler(const Connection *connect, const char *buffer, size_t size, void *)
{
	cout << std::string(buffer, size) << endl;
	std::string temp = createOk("hello world\n");
	char *buf = new char[temp.length()];
	memcpy(buf, temp.c_str(), temp.length());
	WriteMeta toWriteMeta(buf, temp.length());
	//toWrite.emplace_back(connect->fd, toWriteMeta);
	changes.emplace_back(connect->fd, EpollChangeOperation::ADD_WRITE);
	//changes.emplace_back(connect->fd, EpollChangeOperation::CLOSE_IF_NO_WRITE);
	tcpServer.notifyChangeEpoll(changes);
	changes.clear();
	tcpServer.notifyCanWrite(connect->fd, toWriteMeta);
}
void onPeerShutdownHandler(const Connection *connect, void *)
{
	std::vector<std::pair<int, EpollChangeOperation>> change{ {connect->fd, EpollChangeOperation::CLOSE_IT} };
	tcpServer.notifyChangeEpoll(change);
}
#endif // TCP_SERVER_TEST

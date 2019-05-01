#ifdef TCP_SERVER_TEST

#include "TcpServer.h"
#include "tool.h"
#include <iostream>
#include <list>
#include <unistd.h>
using namespace std;

std::vector < std::pair<int, EpollChangeOperation>> changes;

//typedef OnConnectOperation(*OnConnectHandle_t)(const TcpConnection*, void *);
//typedef void(*OnReadHandle_t)(const TcpConnection*, const char *, size_t, void *);
//typedef void(*OnPeerShutdownHandle_t)(const TcpConnection *, void *);
//typedef void(*OnCanWriteHandle_t)(const TcpConnection*, void *);

void deleter(char* p)
{
	delete[] p;
}
int main()
{
	TcpServer tcpServer;
	cout << "setAddressPort result: " << boolalpha
		<< tcpServer.setAddressPort("0.0.0.0", 1234) << endl;
	cout << "getAddress: " << tcpServer.getAddress() << endl;
	cout << "getPort: " << tcpServer.getPort() << endl;
	bool startResult = false;
	tcpServer.onConnect([&tcpServer](const TcpConnection * connection, void* data) -> OnConnectOperation
		{
			return OnConnectOperation::ADD_READ;
		});
	tcpServer.onNewData([&tcpServer](const TcpConnection * connection, const char* data, size_t size, void*)
		{
			if (size == 4 && (strncmp(data, "ping", 4) == 0))
			{
				char* respsond = new char[4];
				memcpy(respsond, "pong", 4);
				tcpServer.notifyCanWrite(connection->fd, { WriteMeta(std::shared_ptr<char>(respsond, deleter), 4) });
			}
			else
			{
				tcpServer.notifyChangeEpoll({ {connection->fd, EpollChangeOperation::CLOSE_IT} });
			}
		});
	tcpServer.onShutdown([](const TcpConnection*, void*) {});
	tcpServer.onCanWrite([](const TcpConnection*, void*) {});
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

#endif // TCP_SERVER_TEST

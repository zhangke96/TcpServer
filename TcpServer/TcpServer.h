#pragma once
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <cassert>
#include "log.h"
#include <map>
#include <deque>
#include <sys/uio.h>
#include <vector>
#include <set>
#include <thread>
#include "aux_class.h"

#define BUFFER_SIZE (80 * 1024)		// 每次读写的最大大小

struct TcpConnection
{
	int fd;
	struct sockaddr_in address;
	struct epoll_event event;
	friend bool operator< (const TcpConnection &lhs, const TcpConnection &rhs);
	friend bool operator== (const TcpConnection &lhs, const TcpConnection &rhs);
};

struct WriteMeta // 记录要write的信息，有时间改成智能指针 todo
{
	const char *buffer;
	int totalLen;
	int toWriteLen;
	WriteMeta() : buffer(nullptr), totalLen(0), toWriteLen(0) {}
	WriteMeta(const char *buf, int len) : buffer(buf), totalLen(len), toWriteLen(len) {}
};

WriteMeta string2WriteMeta(const std::string &str);
enum class OnConnectOperation
{
	CLOSE_IT,
	ADD_READ,
	ADD_WRITE
};
enum class EpollChangeOperation
{
	ADD_WRITE,
	CLOSE_IT,
	CLOSE_IF_NO_WRITE,
};
class TcpServer;

typedef OnConnectOperation (*OnConnectHandle_t)(const TcpConnection*, void *);
typedef void (*OnReadHandle_t)(const TcpConnection*, const char *, size_t, void *);
typedef void(*OnPeerShutdownHandle_t)(const TcpConnection *, void *);

class TcpServer
{
public:
	TcpServer() : serverFd(-1), epollFd(-1), 
		readyForStart(false), onConnectHandler(nullptr),
		onReadHandler(nullptr), data(nullptr), onPeerShutdownHandler(nullptr)
	{
		pipeFds[0] = -1;
		pipeFds[1] = -1;
	}

	~TcpServer();

	TcpServer(const TcpServer&) = delete;
	TcpServer & operator=(const TcpServer&) = delete;

	bool setAddressPort(const std::string &addr, int port)
	{
		struct in_addr tempAddr;
		if (inet_pton(AF_INET, addr.c_str(), &tempAddr) != 1)
		{
			return false;
		}
		if (port >= 65535 || port <= 0)
		{
			return false;
		}
		// 保存到对象的地址结构中
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(tempAddr.s_addr);
		address.sin_port = htons(port);
		readyForStart = true;
		return true;
	}
	void setData(void *newData)
	{
		data = newData;
	}
	std::string getAddress() const
	{
		char buffer[INET_ADDRSTRLEN];
		in_addr_t addr = ntohl(address.sin_addr.s_addr);
		if (inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN) == NULL)
		{
			return "";
		}
		else
		{
			return buffer;
		}
	}
	int getPort() const
	{
		return ntohs(address.sin_port);
	}
	bool onConnect(OnConnectHandle_t handler)
	{
		if (onConnectHandler)
		{
			return false;
		}
		else
		{
			onConnectHandler = handler;
		}
		return true;
	}
	bool onNewData(OnReadHandle_t handler)
	{
		if (onReadHandler)
		{
			return false;
		}
		else
		{
			onReadHandler = handler;
		}
		return true;
	}
	bool onShutdown(OnPeerShutdownHandle_t handler)
	{
		if (onPeerShutdownHandler)
		{
			return false;
		}
		else
		{
			onPeerShutdownHandler = handler;
		}
	}
	bool start()
	{
		if (!readyForStart)
		{
			return false;
		}
		if (!ignoreSignal())	// 忽略SIG_PIPE
		{
			return false;
		}
		int reuse = 1;
		if ((serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (!setNoBlock(serverFd))
		{
			return false;
		}
		if (bind(serverFd, (sockaddr *)&address, sizeof(address)) < 0)  /* bind socket fd to address addr */
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (listen(serverFd, 1024) < 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if ((epollFd = epoll_create1(0)) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			return false;
		}
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = serverFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			return false;
		}
		if (pipe(pipeFds) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			return false;
		}
		if (!setNoBlock(pipeFds[0]))
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			close(pipeFds[0]);
			close(pipeFds[1]);
			return false;
		}
		if (!setNoBlock(pipeFds[1]))
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			close(pipeFds[0]);
			close(pipeFds[1]);
			return false;
		}
		event.data.fd = pipeFds[0];
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, pipeFds[0], &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			return false;
		}
		return true;
	}
	// bool stop(); // 不是很好实现，先不实现

	std::thread runServer(bool ifDetach = true)
	{
		std::thread workThread(&TcpServer::daemonRun, this);
		if (ifDetach)
		{
			workThread.detach();
		}
		return std::move(workThread);
	}
	std::string getError() const
	{
		return errorString;
	}
	bool notifyCanWrite(int fd, const WriteMeta &toWrite)
	{
		AutoMutex m(mutex);
		if (!isVaildConnection(fd))
		{
			return false;
		}
		toWriteTempStorage.emplace_back(fd, toWrite);
		return writePipeToNotify('w');
	}
	bool notifyCanWrite(int fd, const std::deque<WriteMeta> &toWrite)
	{
		AutoMutex m(mutex);
		if (!isVaildConnection(fd))
		{
			return false;
		}
		for (auto c : toWrite)
		{
			toWriteTempStorage.emplace_back(fd, c);
		}
		return writePipeToNotify('w');
	}
	bool notifyChangeEpoll(const std::vector<std::pair<int, EpollChangeOperation>> &changes)
	{
		AutoMutex m(mutex);
		for (auto c : changes)
		{
			if (!isVaildConnection(c.first))
			{
				return false;
			}
		}
		changesTempStorage.insert(changesTempStorage.end(), changes.begin(), changes.end());
		return writePipeToNotify('c');
	}
private:
	struct sockaddr_in address;

	int serverFd;
	int epollFd;
	bool readyForStart;
	std::string errorString;
	std::map<int, TcpConnection> connections;
	std::map<int, std::deque<WriteMeta>> toWriteContents;
	std::set<int> closeWhenWriteFinish;
	std::deque<std::pair<int, WriteMeta>> toWriteTempStorage;
	std::deque<std::pair<int, EpollChangeOperation>> changesTempStorage;
	int pipeFds[2]; // 通过管道TcpServer监听调用者的主动活动，可以在epoll_wait阻塞时快速返回处理
	void *data;	// 保存用户的数据，回调时会传参
	OnConnectHandle_t onConnectHandler;	// TcpServer accept新连接后会回调
	OnReadHandle_t onReadHandler;		// TcpServer read到数据会回调
	OnPeerShutdownHandle_t onPeerShutdownHandler;	// 对端Shutdown会回调

	MutexWrap mutex;

	void recordError(const char * filename, int lineNumber)
	{
		char errorStr[256];
		errorString.assign(filename);
		errorString.append(" : ");
		errorString.append(std::to_string(lineNumber));
		errorString.append(" : ");
		errorString.append(strerror_r(errno, errorStr, 256));	// errno每个线程都有的，线程安全
	}
	bool acceptNewConnection()
	{
		int clfd;
		struct sockaddr_in clientAddr;
		socklen_t length = sizeof(clientAddr);
		while ((clfd = accept(serverFd, (sockaddr *)&clientAddr, &length)) >= 0)
		{
			if (!onConnectHandler)
			{
				close(clfd);
				continue;
			}
			else
			{
				if (!setNoBlock(clfd))
				{
					recordError(__FILE__, __LINE__);
					Log(logger, Logger::LOG_ERROR, errorString);
				}
				connections[clfd] = { clfd, clientAddr, {} };
				addToEpoll(clfd);
				switch (onConnectHandler(&connections[clfd], data))
				{
					// 设置Edge trigger模式
					case OnConnectOperation::CLOSE_IT:
					{
						closeConnection(clfd);
					}
					break;
					case OnConnectOperation::ADD_READ:
					{
						addToRead(clfd);
					}
					break;
					case OnConnectOperation::ADD_WRITE:
					{
						addToWrite(clfd);
					}
					break;
				}
			}
		}
		if (errno = EAGAIN)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	// 设置使用边缘触发的epoll，所有客户端连接都是通过这个添加的
	int addToEpoll(int fd)
	{
		if (!isVaildConnection(fd))
		{
			return 0;
		}
		struct epoll_event &event = connections[fd].event;
		event.events |= EPOLLET;
		event.data.fd = fd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) == -1)
		{
			return -1;
		}
		return 0;
	}
	// close也需要告诉上层，可以区分主动close和tcpserver要close
	int closeConnection(int fd)	
	{
		// 从epoll中移除，然后关闭，然后删除
		if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL) == -1)
		{
			if (errno != ENOENT)
			{
				std::cout << strerror(errno) << std::endl;
				return -1;
			}
		}
		if (shutdown(fd, SHUT_RDWR) < 0)
		{
			return -1;
		}
		if (close(fd) < 0)
		{
			return -1;
		}
		connections.erase(fd);
		auto index = toWriteContents.find(fd);
		if (index != toWriteContents.end())
		{
			for (auto c : index->second)
			{
				if (c.buffer)
				{
					delete[] c.buffer;
				}
			}
			toWriteContents.erase(fd);
		}
		closeWhenWriteFinish.erase(fd);
		return 0;
	}
	int addToRead(int fd)
	{
		if (!isVaildConnection(fd))
		{
			return 0;
		}
		auto &event = connections[fd].event;
		//printEvent(-1, event.events);
		event.events |= EPOLLIN;
		event.events |= EPOLLRDHUP;
		/*************测试**************/
		/*event.events |= EPOLLERR;
		event.events |= EPOLLPRI;*/
		/*******************************/
		event.data.fd = fd;
		if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			Log(logger, Logger::LOG_ERROR, errorString);
			return -1;
		}
		return 0;
	}
	int addToWrite(int fd)
	{
		if (!isVaildConnection(fd))
		{
			return 0;
		}
		auto &event = connections[fd].event;
		//printEvent(-1, event.events);
		event.events |= EPOLLOUT;
		event.data.fd = fd;
		if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			Log(logger, Logger::LOG_ERROR, errorString);
			return -1;
		}
		return 0;
	}
	/*
* 忽略SIGPIPE信号
* 如果向一个已经关闭的socket写会有这个信号
* 或者是一个收到了RST的socket，再写会收到这个信号
* write会返回EPIPE
*/
	bool ignoreSignal()
	{
		sigset_t signals;
		if (sigemptyset(&signals) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (sigaddset(&signals, SIGPIPE) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		return true;
	}

	bool setNoBlock(int fd)
	{
		int val;
		if ((val = fcntl(fd, F_GETFL, 0)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if ((val = fcntl(fd, F_SETFL, val | O_NONBLOCK)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;;
		}
		return true;
	}

	void printEvent(int fd, uint32_t events)
	{
		std::cout << "***********************************************" << std::endl;
		std::cout << "fd: " << fd << std::endl;
		if (connections.find(fd) != connections.cend())	// 有可能是服务端套接字和通知管道套接字，就不能输出地址了
		{
			std::cout << inet_ntoa(connections[fd].address.sin_addr) << " : " << ntohs(connections[fd].address.sin_port) << std::endl;
		}
		if (events & EPOLLIN)
			std::cout << "EPOLLIN" << std::endl;
		if (events & EPOLLPRI)
			std::cout << "EPOLLPRI" << std::endl;
		if (events & EPOLLOUT)
			std::cout << "EPOLLOUT" << std::endl;
		if (events & EPOLLRDNORM)
			std::cout << "EPOLLRDNORM" << std::endl;
		if (events & EPOLLRDBAND)
			std::cout << "EPOLLRDBAND" << std::endl;
		if (events & EPOLLWRNORM)
			std::cout << "EPOLLWRNORM" << std::endl;
		if (events & EPOLLWRBAND)
			std::cout << "EPOLLWRBAND" << std::endl;
		if (events & EPOLLMSG)
			std::cout << "EPOLLMSG" << std::endl;
		if (events & EPOLLERR)
			std::cout << "EPOLLERR" << std::endl;
		if (events & EPOLLHUP)
			std::cout << "EPOLLHUP" << std::endl;
		if (events & EPOLLRDHUP)
			std::cout << "EPOLLRDHUP" << std::endl;
		if (events & EPOLLWAKEUP)
			std::cout << "EPOLLWAKEUP" << std::endl;
		if (events & EPOLLONESHOT)
			std::cout << "EPOLLONESHOT" << std::endl;
		if (events & EPOLLET)
			std::cout << "EPOLLET" << std::endl;
		std::cout << "***********************************************" << std::endl;
	}

	bool writeContent(int fd)
	{
		if (toWriteContents.find(fd) == toWriteContents.end())
		{
			return true;
		}
		else if (toWriteContents[fd].size() == 0)
		{
			return true;
		}
		else
		{
			// 使用writev
			auto &toWriteDeque = toWriteContents[fd];
			struct iovec *toWriteIovec = new struct iovec[toWriteDeque.size()];
			int remainSize = BUFFER_SIZE;
			int i = 0;
			while (remainSize > 0 && i < toWriteDeque.size())
			{
				auto const &writeMeta = toWriteDeque[i];
				toWriteIovec[i].iov_base = (void *)(writeMeta.buffer + writeMeta.totalLen - writeMeta.toWriteLen);
				if (remainSize >= writeMeta.toWriteLen)
				{	
					toWriteIovec[i].iov_len = writeMeta.toWriteLen;
					remainSize -= writeMeta.toWriteLen;
				}
				else
				{
					toWriteIovec[i].iov_len = remainSize;
					remainSize = 0;
				}
				++i;
			}
			ssize_t size = writev(fd, toWriteIovec, i);
			delete[] toWriteIovec;
			if (size == -1)
			{
				if (errno == EAGAIN)
				{
					
				}
				else
				{
					return false;
				}
			}
			else // 开始修改Meta数据并且移除写完的Meta
			{
				int i = 0;
				auto index = toWriteDeque.begin();
				while (size > 0)
				{
					if (index->toWriteLen > 0)
					{
						if (size >= index->toWriteLen)
						{
							size -= index->toWriteLen;
							delete[] index->buffer;
							toWriteDeque.pop_front();
							index = toWriteDeque.begin();
						}
						else
						{
							index->toWriteLen -= size;
						}
					}
					else
					{
						// 不应该有问题的
						Log(logger, Logger::LOG_ERROR, "should not run here");
						exit(-1);
					}
				}
			}
			if (toWriteDeque.size() == 0)
			{
				if (closeWhenWriteFinish.find(fd) != closeWhenWriteFinish.end())
				{
					closeConnection(fd);
				}
			}
			return true;
		}
	}

	void daemonRun()
	{
		struct epoll_event *toHandleEvents;
		toHandleEvents = new struct epoll_event[1024];
		assert(toHandleEvents);
		for (; ;)
		{
			int epollRet = epoll_wait(epollFd, toHandleEvents, 1024, -1);
			if (epollRet < 0)
			{
				recordError(__FILE__, __LINE__);
				Log(logger, Logger::LOG_ERROR, errorString);
			}
			else
			{
				for (int i = 0; i < epollRet; ++i)
				{
					printEvent(toHandleEvents[i].data.fd, toHandleEvents[i].events);
					if (toHandleEvents[i].data.fd == serverFd)	// 监听的serverFd
					{
						if (toHandleEvents[i].events & EPOLLIN)
						{
							if (!acceptNewConnection())
							{
								recordError(__FILE__, __LINE__);
								Log(logger, Logger::LOG_ERROR, errorString);
							}
						}
					}
					else if (toHandleEvents[i].data.fd == pipeFds[0])	// 调用者通知管道fd
					{
						if (toHandleEvents[i].events & EPOLLIN)
						{
							handleNotify();
						}
					}
					else	// 处理客户端的连接
					{
						int fd = toHandleEvents[i].data.fd;
						if (connections.find(fd) == connections.end())
						{
							Log(logger, Logger::LOG_ERROR, "fd not seen before");
							continue;
						}
						if (toHandleEvents[i].events & EPOLLIN || toHandleEvents[i].events & EPOLLHUP)
						{
							char *buffer = nullptr;
							buffer = new char[BUFFER_SIZE];
							ssize_t number = -1;
							while ((number = read(fd, buffer, BUFFER_SIZE)) > 0)
							{
								if (onReadHandler)
								{
									auto c = connections[fd];
									onReadHandler(&(connections[fd]), buffer, number, data);
								}
							}
							if (number < 0)
							{
								if (errno == EAGAIN)
								{

								}
								else
								{
									recordError(__FILE__, __LINE__);
									Log(logger, Logger::LOG_ERROR, errorString);
								}
							}
							else if (number == 0) // 对端关闭了连接，通知调用者
							{
								// closeConnection(fd);
								if (onPeerShutdownHandler)
								{
									onPeerShutdownHandler(&(connections[fd]), data);
								}
							}
							delete[] buffer;
						}
						if (toHandleEvents[i].events & EPOLLOUT)
						{
							if (!writeContent(fd))
							{
								recordError(__FILE__, __LINE__);
								Log(logger, Logger::LOG_ERROR, errorString);
							}
						}
					}
				}
			}
		}
	}
	void handleNotify()
	{
		for (; ;)
		{
			char commandStr[1];
			int ret = -1;
			if ((ret = read(pipeFds[0], commandStr, 1)) != 1)
			{
				if (ret == -1)
				{
					if (errno == EAGAIN)
					{
						
					}
					else
					{
						recordError(__FILE__, __LINE__);
					}
				}
				break;
			}
			else
			{
				handleSpecifyNotify(commandStr[0]);
			}
		}
	}
	bool handleSpecifyNotify(char command)
	{
		switch (command)
		{
			case 'w':	// 有新的写请求
			{
				AutoMutex m(mutex);
				//toWriteContents.insert(toWriteContents.end(), toWriteTempStorage)
				for (auto c : toWriteTempStorage)
				{
					auto &writeDeque = toWriteContents[c.first];
					writeDeque.push_back(c.second);
				}
				toWriteTempStorage.clear();
			}
			break;
			case 'c':	// 有改变Epoll状态的请求
			{
				AutoMutex m(mutex);
				for (auto c : changesTempStorage)
				{
					if (c.second == EpollChangeOperation::ADD_WRITE)
					{
						addToWrite(c.first);
					}
					else if (c.second == EpollChangeOperation::CLOSE_IF_NO_WRITE)
					{
						closeWhenWriteFinish.insert(c.first);
					}
					else if (c.second == EpollChangeOperation::CLOSE_IT)	// 调用者要求立刻关闭连接
					{
						closeConnection(c.first);
					}
				}
				changesTempStorage.clear();
			}
			break;
		}
		return true;
	}
	bool writePipeToNotify(char command)
	{
		char commandStr[1];
		commandStr[0] = command;
		int ret = -1;
		if ((ret = write(pipeFds[1], commandStr, 1)) != 1)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		else
		{
			return true;
		}
	}
	bool isVaildConnection(int fd)
	{
		return connections.find(fd) != connections.end();
	}
	
};


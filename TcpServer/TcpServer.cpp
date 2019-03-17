#include "TcpServer.h"

TcpServer::~TcpServer()
{
}

bool operator<(const TcpConnection &lhs, const TcpConnection &rhs)
{
	return lhs.fd < rhs.fd;
}

WriteMeta string2WriteMeta(const std::string &str)
{
	char *buf = new char[str.length()];
	memcpy(buf, str.c_str(), str.length());
	return WriteMeta(buf, str.length());
}

bool operator== (const TcpConnection &lhs, const TcpConnection &rhs)
{
	return lhs.fd == rhs.fd 
		&& strncmp((const char *)&(lhs.address), (const char *)&(rhs.address), sizeof(struct sockaddr_in)) == 0;
}
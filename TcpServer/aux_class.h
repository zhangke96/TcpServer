#ifndef AUX_CLASS
#define AUX_CLASS
#include <memory>
#include <unistd.h>
#include "tool.h"
#include <pthread.h>
#include <sys/stat.h>
#include "log.h"
class fdwrap
{
	/* 用来包装fd(file descriptor),可以共享fd,自动计数，没有引用时自动释放 */
	// 如果是套接字还不能直接close，得想想如何修改 2018/12/25
public:
	fdwrap() : fdsp(new int(-1), close) {}   // default constructor, assign a invaild fd -1
	explicit fdwrap(int filed) : fdsp(new int(filed), close) {}
	ssize_t read(char *buf, size_t size)
	{
		return ::read(*fdsp, buf, size);
	}
	ssize_t write(char *buf, size_t size)
	{
		return ::write(*fdsp, buf, size);
	}
	int getfd() const
	{
		return *fdsp;
	}
	bool endOfFile() const
	{
		struct stat statinfo;
		if (fstat(*fdsp, &statinfo) != 0)
		{
			Log(logger, Logger::LOG_ERROR, "error when fstat");
			return true;
		}
		off_t fileoffset;
		if ((fileoffset = lseek(*fdsp, 0, SEEK_CUR)) == -1)
		{
			Log(logger, Logger::LOG_ERROR, "can't lseek");
			return true;
		}
		return (fileoffset == statinfo.st_size);
	}
	void reset(int newfd)
	{
		fdsp.reset(new int(newfd), close);
	}
	fdwrap(const fdwrap &rhs)
		: fdsp(rhs.fdsp)
	{}
	fdwrap& operator=(const fdwrap &rhs)
	{
		if (this != &rhs)
		{
			fdsp = rhs.fdsp;
		}
		return *this;
	}
	fdwrap(fdwrap &&rhs) noexcept
		: fdsp(std::move(rhs.fdsp))
	{
	}
	fdwrap& operator=(fdwrap &&rhs) noexcept
	{
		if (this != &rhs)
		{
			fdsp = std::move(rhs.fdsp);
		}
		return *this;
	}
private:
	// int fd;
	std::shared_ptr<int> fdsp;
	static void close(int *fdp)
	{
		if (*fdp == -1)
		{
			delete fdp;
			return;
		}
		if (::close(*fdp) != 0)
		{
			Log(logger, Logger::LOG_ERROR, "error when close fd: " + std::to_string(*fdp));
		}
		delete fdp;
	}
};

class MutexWrap
{
public:
	MutexWrap()
	{
		pthread_mutex_init(&mutex, NULL);
	}
	~MutexWrap()
	{
		pthread_mutex_destroy(&mutex);
	}
	MutexWrap(const MutexWrap&) = delete;
	MutexWrap& operator=(const MutexWrap&) = delete;
	int lock()
	{
		return pthread_mutex_lock(&mutex);
	}
	int unlock()
	{
		return pthread_mutex_unlock(&mutex);
	}
private:
	pthread_mutex_t mutex;
};

class AutoMutex
{
public:
	explicit AutoMutex(MutexWrap &amutex) : mutex(amutex)
	{
		mutex.lock();
	}
	~AutoMutex()
	{
		mutex.unlock();
	}
	AutoMutex(const AutoMutex&) = delete;
	AutoMutex& operator=(const AutoMutex&) = delete;
private:
	MutexWrap &mutex;
};

#endif // !AUX_CLASS


#ifndef ZK_HTTP_LOG
#define ZK_HTTP_LOG
#include <iostream>
#include <fstream>
#include <string>
#include <sys/time.h>
#include <pthread.h>
#include <cassert>

class Logger
{
public:
	enum LogLevel
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARN,
		LOG_ERROR,
	};
	Logger(const std::string &logFileName, LogLevel defaultLevel = LOG_DEBUG)
		: logStream(logFileName, std::ostream::app)
	{
		setNewLevel(defaultLevel);
	}
	void reOpen(const std::string &newLogFileName)
	{
		if (logStream.is_open())
		{
			logStream.close();
			logStream.clear();
		}
		logStream.open(newLogFileName);
	}
	Logger(const Logger&) = delete; // ½ûÖ¹¿½±´¹¹Ôìº¯Êý
	Logger& operator=(const Logger&) = delete; // ½ûÖ¹¿½±´¸³ÖµÔËËã·û
	LogLevel getLevel() { return displayLevel; }
	void setLevel(LogLevel newLevel)
	{
		setNewLevel(newLevel);
	}
	void printLog(LogLevel level, const std::string& msg, const char *filename, int linenumber);
private:
	std::ofstream logStream;
	LogLevel displayLevel;
	void setNewLevel(LogLevel level)
	{
		assert(level == LOG_DEBUG || level == LOG_INFO || level == LOG_WARN || level == LOG_ERROR);
		displayLevel = level;
	}
};

//use marco to avoid always input __FILE__, __LINE__
#define Log(logger, level, message) \
do {\
	(logger).printLog((level), (message), __FILE__, __LINE__); \
} while(0)

extern Logger logger;

#endif // !ZK_HTTP_LOG


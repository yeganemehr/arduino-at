#ifndef ARDUINO_AT_ATConnection
#define ARDUINO_AT_ATConnection

#include <WString.h>
#include <Stream.h>
#include <queue>
#include <Promise.hpp>
#include <EventEmitter.hpp>
#include "events/ATNotificationEvent.hpp"
#include "exceptions/GeneralATError.hpp"

struct SetCommand
{
	char *variable;
	char *value;
};
struct GetCommand
{
	char *variable;
};
struct TestCommand
{
	char *variable;
};
struct ExecuteCommand
{
	char *command;
	char *secondPart = nullptr;
};

union ATCommand
{
	SetCommand set;
	GetCommand get;
	TestCommand test;
	ExecuteCommand execute;
};

class ATConnection : public EventEmitter
{
public:
	ATConnection(Stream *stream);
	Promise<void> *setValue(const char *variable, const char *value) noexcept;
	Promise<String> *getValue(const char *variable) noexcept;
	Promise<String> *test(const char *variable) noexcept;
	Promise<String> *execute(const char *command) noexcept;
	Promise<String> *execute(const char *command, const char *secondPart) noexcept;
	void communicate() noexcept;

private:
	Stream *stream;
	String buffer;
	enum State : uint8_t
	{
		WRITING,
		READING_NOTIFICATION,
		READING_RESPONSE,
	} state = READING_NOTIFICATION;

	struct CommandQueueItem
	{
		enum Type : uint8_t
		{
			SET,
			GET,
			TEST,
			EXECUTE,
		} type;
		ATCommand command;
		void *promise;
	};
	std::queue<CommandQueueItem> commandQueue;

	void read() noexcept;
	void write() noexcept;
	bool parse();
	bool parseResponse() noexcept;
	void putTheCommandInBuffer(CommandQueueItem *command);
	void putTheCommandInBuffer(GetCommand *command);
	void putTheCommandInBuffer(SetCommand *command);
	void putTheCommandInBuffer(TestCommand *command);
	void putTheCommandInBuffer(ExecuteCommand *command);
};

#endif
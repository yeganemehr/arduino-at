#ifndef ARDUINO_AT_ATConnection
#define ARDUINO_AT_ATConnection

#include <WString.h>
#include <Stream.h>
#include <queue>
#include <Promise.hpp>
#include <EventEmitter.hpp>
#include "events/ATNotificationEvent.hpp"
#include "exceptions/GeneralATError.hpp"
#include <Arduino.h>
#include <ctime>

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
	char *secondPart;
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
	Promise<void> *setValue(const char *variable, const char *value, uint8_t timeuot = 2) noexcept;
	Promise<String> *getValue(const char *variable, uint8_t timeuot = 2) noexcept;
	Promise<String> *test(const char *variable, uint8_t timeuot = 2) noexcept;
	Promise<String> *execute(const char *command, uint8_t timeuot = 2) noexcept;
	Promise<String> *execute(const char *command, const char *secondPart, uint8_t timeuot = 20) noexcept;
	inline void communicate() noexcept {
		State state;
		do
		{
			state = this->state;
			switch (this->state)
			{
			case State::WRITING:
				this->write();
				break;
			case State::READING_NOTIFICATION:
			case State::READING_RESPONSE:
				this->read();
				break;
			}
#ifdef ESP8266
			yield();
#endif
		} while (state != this->state);
	}

private:
	Stream *stream;
	String buffer;
	std::time_t sentCommandAt = 0;
	
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
		uint8_t timeout;
		ATCommand command;
		void *promise;
	};
	std::queue<CommandQueueItem> commandQueue;

	void read() noexcept;
	void write() noexcept;
	bool parseResponse() noexcept;
	bool parseNotification() noexcept;
	inline bool parse() noexcept
	{
		switch (state)
		{
		case State::READING_RESPONSE:
			return parseResponse();
		case State::READING_NOTIFICATION:
			return parseNotification();
		default:
			assert(false);
		}
	}

	void putTheCommandInBuffer(GetCommand *command);
	void putTheCommandInBuffer(SetCommand *command);
	void putTheCommandInBuffer(TestCommand *command);
	void putTheCommandInBuffer(ExecuteCommand *command);

	inline void putTheCommandInBufferIfCan() {
		if (state != State::READING_NOTIFICATION || commandQueue.empty() || !buffer.isEmpty())
		{
			return;
		}
		putTheCommandInBuffer();
		state = State::WRITING;
	}
	inline void putTheCommandInBuffer() {
		CommandQueueItem &item = commandQueue.front();
		putTheCommandInBuffer(&item);
		sentCommandAt = item.timeout ? std::time(nullptr) : 0;
	}
	inline void putTheCommandInBuffer(CommandQueueItem *item) {
		switch (item->type)
		{
		case CommandQueueItem::Type::GET:
			putTheCommandInBuffer(&item->command.get);
			break;
		case CommandQueueItem::Type::SET:
			putTheCommandInBuffer(&item->command.set);
			break;
		case CommandQueueItem::Type::TEST:
			putTheCommandInBuffer(&item->command.test);
			break;
		case CommandQueueItem::Type::EXECUTE:
			putTheCommandInBuffer(&item->command.execute);
			break;
		}
	}
	inline void checkForCommandTimeout() {
		if (!sentCommandAt)
		{
			return;
		}
		std::time_t now = std::time(nullptr);
		CommandQueueItem &item = commandQueue.front();
		if (now - sentCommandAt < item.timeout)
		{
			return;
		}
		switch (item.type)
		{
		case CommandQueueItem::Type::GET:
		case CommandQueueItem::Type::TEST:
		case CommandQueueItem::Type::EXECUTE:
		{
			((Promise<String> *)item.promise)->reject(std::exception());
		}
		break;
		case CommandQueueItem::Type::SET:
			((Promise<void> *)item.promise)->reject(std::exception());
			break;
		}
		state = State::READING_NOTIFICATION;
		sentCommandAt = 0;
		commandQueue.pop();
	}
};

#endif

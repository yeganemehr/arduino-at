#include "ATConnection.hpp"
#include <assert.h>

ATConnection::ATConnection(Stream *stream) : stream(stream)
{
}
void ATConnection::putTheCommandInBuffer(CommandQueueItem *item)
{
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
void ATConnection::putTheCommandInBuffer(GetCommand *command)
{
	assert(!buffer.length());
	assert(command->variable);
	auto len = strlen(command->variable);
	buffer.reserve(3 + len + 1 + 1);
	buffer.concat("AT+");
	buffer.concat(command->variable, len);
	buffer.concat("?\r");
	delete[] command->variable;
	command->variable = nullptr;
}
void ATConnection::putTheCommandInBuffer(SetCommand *command)
{
	assert(!buffer.length());
	assert(command->variable);
	assert(command->value);
	auto lenVar = strlen(command->variable);
	auto lenVal = strlen(command->value);
	buffer.reserve(3 + lenVar + 1 + lenVal + 1);
	buffer.concat("AT+");
	buffer.concat(command->variable, lenVar);
	buffer.concat('=');
	buffer.concat(command->value, lenVal);
	buffer.concat('\r');
	delete[] command->variable;
	command->variable = nullptr;
	delete[] command->value;
	command->value = nullptr;
}
void ATConnection::putTheCommandInBuffer(TestCommand *command)
{
	assert(!buffer.length());
	assert(command->variable);
	auto len = strlen(command->variable);
	buffer.reserve(3 + len + 2 + 1);
	buffer.concat("AT+");
	buffer.concat(command->variable, len);
	buffer.concat("=?\r");
	delete[] command->variable;
	command->variable = nullptr;
}
void ATConnection::putTheCommandInBuffer(ExecuteCommand *command)
{
	assert(!buffer.length());
	size_t len;

	if (command->command) {
		assert(command->command);
		len = strlen(command->command);
		buffer.reserve(len + 1);
		buffer.concat(command->command, len);
		buffer.concat('\r');
		delete[] command->command;
		command->command = nullptr;
		return;
	}

	if (command->secondPart)
	{
		assert(command->secondPart);
		len = strlen(command->secondPart);
		buffer.reserve(len + 1);
		buffer.concat(command->secondPart, len);
		buffer.concat('\x1A');

		delete[] command->secondPart;
		command->secondPart = nullptr;
		return;
	}

	panic();
}
Promise<String> *ATConnection::getValue(const char *variable) noexcept
{
	Promise<String> *promise = new Promise<String>();
	auto variableLength = strlen(variable);
	auto ptr = new char[variableLength + 1];
	memcpy(ptr, variable, variableLength + 1);
	commandQueue.push(CommandQueueItem{CommandQueueItem::Type::GET, {.get = {.variable = ptr}}, promise});
	return promise;
}
Promise<String> *ATConnection::execute(const char *command) noexcept
{
	Promise<String> *promise = new Promise<String>();
	auto commandLength = strlen(command);
	char *ptr = new char[commandLength + 1];
	memcpy(ptr, command, commandLength + 1);
	commandQueue.push(CommandQueueItem{
		.type = CommandQueueItem::Type::EXECUTE,
		.command = {
			.execute = {
				.command = ptr,
				.secondPart = nullptr
			}
		},
		.promise = promise
	});
	return promise;
}
Promise<String> *ATConnection::execute(const char *command, const char *secondPart) noexcept {
	Promise<String> *promise = new Promise<String>();
	auto commandLength = strlen(command);
	auto secondPartLength = strlen(secondPart);
	char *cPtr = new char[commandLength + 1];
	char *sPtr = new char[secondPartLength + 1];
	memcpy(cPtr, command, commandLength + 1);
	memcpy(sPtr, secondPart, secondPartLength + 1);
	commandQueue.push(CommandQueueItem{
		.type = CommandQueueItem::Type::EXECUTE,
		.command = {
			.execute = {
				.command = cPtr,
				.secondPart = sPtr
			}
		},
		.promise = promise
	});
	return promise;
}
Promise<String> *ATConnection::test(const char *variable) noexcept
{
	Promise<String> *promise = new Promise<String>();
	auto variableLength = strlen(variable);
	auto ptr = new char[variableLength + 1];
	memcpy(ptr, variable, variableLength + 1);
	commandQueue.push(CommandQueueItem{
		.type = CommandQueueItem::Type::TEST,
		.command = {.test = {.variable = ptr}},
		.promise = promise
	});
	if (commandQueue.size() == 1) {
		this->state = State::WRITING;
	}
	return promise;
}
Promise<void> *ATConnection::setValue(const char *variable, const char *value) noexcept
{
	Promise<void> *promise = new Promise<void>();
	auto variableLength = strlen(variable);
	auto valueLength = strlen(value);
	auto pVar = new char[variableLength + 1];
	auto pVal = new char[valueLength + 1];
	memcpy(pVar, variable, variableLength + 1);
	memcpy(pVal, value, valueLength + 1);
	commandQueue.push(CommandQueueItem{CommandQueueItem::Type::SET, {.set = {.variable = pVar, .value = pVal}}, promise});
	return promise;
}

void ATConnection::communicate() noexcept
{
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
	} while (state != this->state);
}

void ATConnection::write() noexcept
{
	assert(state == State::WRITING);
	if (!stream->availableForWrite())
	{
		return;
	}
	auto length = buffer.length();
	assert(length);
	size_t written = stream->write(buffer.c_str(), length);
	if (written == length)
	{
		buffer.clear();
		state = State::READING_RESPONSE;
	}
	else
	{
		buffer.remove(0, written);
	}
}
void ATConnection::read() noexcept
{
	assert(state == State::READING_NOTIFICATION || state == State::READING_RESPONSE);
	auto available = stream->available();
	while (available > 0)
	{
		char buf[256];
		buf[255] = 0;
		available = stream->readBytes(buf, sizeof(buf) - 1);
		uint8_t begin = 0;
		while (isspace(buf[begin]))
		{
			begin++;
		}
		available -= begin;
		if (!available)
		{
			continue;
		}
		buffer.concat(buf + begin, available);
		available = stream->available();
	}
	if (buffer.length()) {
		while ((state == State::READING_NOTIFICATION || state == State::READING_RESPONSE) && buffer.length() && parse())
		{
			if (state == State::READING_RESPONSE) {
				state = State::READING_NOTIFICATION;
			}
			if (!commandQueue.empty())
			{
				commandQueue.pop();
			}
		}
	}
	else if (state == State::READING_NOTIFICATION  && !commandQueue.empty())
	{
		state = State::WRITING;
		putTheCommandInBuffer(&commandQueue.front());
	}
}

bool ATConnection::parse()
{
	assert(state == State::READING_NOTIFICATION || state == State::READING_RESPONSE);
	assert(!buffer.isEmpty());
	if (state == State::READING_RESPONSE) {
		return parseResponse();
	}
	auto pos = buffer.indexOf('\n');
	if (pos < 0)
	{
		return false;
	}
	ATNotificationEvent event(buffer.substring(0, pos - 1));
	this->emit(&event);
	buffer.remove(0, pos + 1);
	return true;
}

bool ATConnection::parseResponse() noexcept
{
	assert(state == State::READING_RESPONSE);
	assert(!commandQueue.empty());

	auto item = commandQueue.front();
	auto pos = buffer.indexOf(F("OK\r\n"));
	if (pos != -1)
	{
		if (item.promise)
		{
			switch (item.type)
			{
			case CommandQueueItem::Type::GET:
			case CommandQueueItem::Type::TEST:
			case CommandQueueItem::Type::EXECUTE:
				((Promise<String> *)item.promise)->resolve(buffer.substring(0, pos));
				break;
			case CommandQueueItem::Type::SET:
				((Promise<void> *)item.promise)->resolve();
				break;
			}
		}
		buffer.remove(0, pos + 4);
		return true;
	}
	pos = buffer.indexOf(F("ERROR\r\n"));
	if (pos != -1)
	{
		if (item.promise)
		{
			switch (item.type)
			{
			case CommandQueueItem::Type::GET:
			case CommandQueueItem::Type::TEST:
			case CommandQueueItem::Type::EXECUTE:
				((Promise<String> *)item.promise)->reject(GeneralATError());
				break;
			case CommandQueueItem::Type::SET:
				((Promise<void> *)item.promise)->reject(GeneralATError());
				break;
			}
		}
		buffer.remove(0, pos + 7);
		return true;
	}
	pos = buffer.indexOf(F(">"));
	if (pos != -1)
	{
		assert(item.type == CommandQueueItem::Type::EXECUTE);
		assert(item.command.execute.secondPart);
		buffer.clear();
		putTheCommandInBuffer(&item.command.execute);
		state = State::WRITING;
	}
	return false;
}
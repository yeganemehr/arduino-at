#include "ATConnection.hpp"
#include <assert.h>

ATConnection::ATConnection(Stream *stream) : stream(stream)
{
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
	putTheCommandInBufferIfCan();
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
	putTheCommandInBufferIfCan();
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
	putTheCommandInBufferIfCan();
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
	putTheCommandInBufferIfCan();
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
	putTheCommandInBufferIfCan();
	return promise;
}

void ATConnection::write() noexcept
{
	auto length = buffer.length();
	size_t written = stream->write(buffer.c_str(), length);
	Serial.printf("AT > %s\n", buffer.substring(0, written).c_str());
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
	char buf[255];
	uint8_t countRead = stream->read((uint8_t *)buf, sizeof(buf));
	if (!countRead)
	{
		return;
	}
	if (!buffer.concat(buf, countRead)) {
		buffer.clear();
		return;
	}
	if (!memchr(buf, '\n', countRead))
	{
		return;
	}
	bool parseSuccess;
	do {
		parseSuccess = parse();
		if (!parseSuccess)
		{
			return;
		}
		if (state == State::READING_RESPONSE)
		{
			state = State::READING_NOTIFICATION;

			if (!commandQueue.empty())
			{
				commandQueue.pop();
			}
		}
	} while ((state == State::READING_NOTIFICATION || state == State::READING_RESPONSE) && buffer.length());
	if (state == State::READING_NOTIFICATION && parseSuccess && !commandQueue.empty())
	{
		// If parseSuccess is true && state is READING_NOTIFICATION then buffer is definitely is empty
		putTheCommandInBuffer();
		state = State::WRITING;
	}
}
bool ATConnection::parseResponse() noexcept
{
	assert(state == State::READING_RESPONSE);
	assert(!commandQueue.empty());

	auto item = commandQueue.front();
	int pos = buffer.indexOf("OK\r\n");
	if (pos != -1)
	{
		if (item.promise)
		{
			switch (item.type)
			{
			case CommandQueueItem::Type::GET:
			case CommandQueueItem::Type::TEST:
			case CommandQueueItem::Type::EXECUTE:
				{
					const char *cStr = buffer.c_str();
					uint8_t offsetStart = 0;
					uint8_t offsetEnd = 0;
					for (; offsetStart < pos && isspace(cStr[offsetStart]); offsetStart++);
					for (; offsetEnd < pos - offsetStart && isspace(cStr[pos - offsetEnd - 1]); offsetEnd++);
					((Promise<String> *)item.promise)->resolve(buffer.substring(offsetStart, pos - offsetEnd));
				}
				break;
			case CommandQueueItem::Type::SET:
				((Promise<void> *)item.promise)->resolve();
				break;
			}
		}
		buffer.remove(0, pos + 4);
		return true;
	}
	pos = buffer.indexOf("ERROR\r\n");
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
	pos = buffer.indexOf('>');
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

bool ATConnection::parseNotification() noexcept
{
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
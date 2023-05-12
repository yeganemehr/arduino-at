#ifndef ARDUINO_AT_EVENTS_ATNOTIFICATIONEVENT
#define ARDUINO_AT_EVENTS_ATNOTIFICATIONEVENT

#include <Event.hpp>
#include <WString.h>

class ATNotificationEvent : public Event
{
public:
	constexpr static event_type_t type = 0xff020201;
	const String &content;

	ATNotificationEvent(const String &content) : content(content) {}
	event_type_t getType() const
	{
		return type;
	}
};

#endif
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/yeganemehr/library/arduino-at.svg)](https://registry.platformio.org/libraries/yeganemehr/arduino-at)
[![Build Examples](https://github.com/yeganemehr/arduino-at/actions/workflows/build-examples.yml/badge.svg)](https://github.com/yeganemehr/arduino-at/actions/workflows/build-examples.yml)

# General Information
I tried to build a fluent API for communication with GSM modems in a generic and efficient way.  
There are plenty of GSM modem clients out there but very few of them are truthly asynchronous and others just using `delay()` in their code which can problems in other parts of process.

Anyway technically this code run on any boards which can run Ardunio framework but I did tested it only on esp8266.

# Quickstart

Let's say we want to test if our modem is up and running and if AT Echo was enabled, we'll turn it off.   
In this example I used SoftwareSerial on D3 & D4 pins but you are free to use HardwareSerial or even a Tcp stream!
```c++
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ATConnection.hpp>

SoftwareSerial swSerial(D3, D4);
ATConnection at(&swSerial);

void setup() {
	Serial.begin(9600);
	swSerial.begin(9600);
	delay(1000);
	Serial.println("checking the connection:");
	at.execute("AT")
		->onSuccess([](const String &result) {
			bool echo = result.startsWith("AT");
			Serial.print("Modem successfully respond and AT Echo is ");
			Serial.println(echo ? "on, i'm going to turn it off!" : "off.");
			if (echo) {
				at.execute("ATE0")->freeOnFinish();
			}
		})
		->onFail([](const std::exception &e) {
			Serial.printf("Modem respond with error");
		})
		->freeOnFinish();
}

void loop() {
	at.communicate();
}

```
You can build and upload this code to your board quickly with [Ready-To-Use Example](examples/esp8266-arduino)

## Important to Remember
Currently the library using heap memory to construct promises so you must take care of free the memory of promise.   
Dont forget that deleting a promise before it's finished can cause [undefined behavior](https://en.wikipedia.org/wiki/Undefined_behavior).   
There is a `void Promise<T>::freeOnFinish()` method can certainly help you with this matter.

## Methods & Helpers

**ATConnection::ATConnection(Stream *stream)**:  
The connection constructor needs a stream, It's usully is a Software or Hardware serial but technically it even can be a TCP stream!

**Promise\<void\> *ATConnection::setValue(char *variable, char *value)**:  
It's basically an alias for `ATConnection::execute("AT+${variable}=${value}")`.

**Promise\<String\> *ATConnection::getValue(char *variable)**:  
It's basically an alias for `ATConnection::execute("AT+${variable}?")`.

**Promise\<String\> *ATConnection::test(char *variable)**:  
It's basically an alias for `ATConnection::execute("AT+${variable}=?")`.

**Promise\<String\> *ATConnection::execute(char *command)**:  
Sends `command` to the modem and waits for it's response.  
If the response ends with "OK<CRLF>" promise will resolve with any data after sending the command and receive the `OK`.  
If the response ends with "ERROR<CRLF>" promise will reject any data after sending the command and receive the `ERROR`.

**Promise\<String\> *ATConnection::execute(char *command, char *secondPart)**:  
It's like prevouis form but if there is a need to send second part of payload, it will send it.   
Executing commands like `AT+CMGS` needs this form.

**void ATConnection::communicate()**:  
Read from / Write to stream.
You should put it in `loop()` method.


# TODO
* Add listening to NotificationEvent to documention.
* Unit tests.

# License
The library is licensed under [MIT](LICENSE)
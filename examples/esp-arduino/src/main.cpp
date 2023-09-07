#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ATConnection.hpp>

SoftwareSerial swSerial(D3, D4);
ATConnection at(&swSerial);

void setup() {
	Serial.begin(9600);
	swSerial.begin(9600);
	delay(1000);

	// Setup event listener
	at.on(ATNotificationEvent::type, [](const Event *env) {
		auto notification = (const ATNotificationEvent *)env;
		Serial.printf("AT Notfication: \"%s\"\n", notification->content.c_str());
	});

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
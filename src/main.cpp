#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>
#include <IRrecv.h>
#include "IRsend.h"
#include <IRremoteESP8266.h>
#include <ArduinoJson.h>
#include <IRutils.h>
#include <ir_Gree.h>
extern "C" {
  #include "user_interface.h"
}

const unsigned int NODE_ID = 1;

const uint16_t LED_PIN = 4;
const char * ssid = "bang-pow";
const char * pass = "mastercard"; // I don't mind people using my wifi if they are in the area

const unsigned long ONE_HOUR_IN_MICRO = 3.6e9;
const unsigned long ONE_HOUR_IN_SECONDS = 60 * 60;

const unsigned int HOUR = 0;
const unsigned int MINUTE = 1;
const unsigned int POWER = 2;
const unsigned int TEMP = 3;

typedef struct {
  unsigned int counter;
  unsigned long lastSleepTime;
  unsigned long lastTime;
  unsigned int lastScheduleIndex;
  char schedule[300];
} stateStruct __attribute__((aligned(4)));
stateStruct state;

IPAddress timeServerIP;
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
WiFiUDP udp;
unsigned int localPort = 2390;

// from sketch examples
void sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

unsigned long fetchNTPTime() {
  udp.begin(localPort);
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  delay(1000);

  int cb;
  int counter = 0;
  while(!(cb = udp.parsePacket())) {
    Serial.print(".");
    delay(200);
    counter ++;
    if(counter > 100)
      return fetchNTPTime();
  }
  Serial.println("");

  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  const unsigned long seventyYears = 2208988800UL;
  unsigned long epoch = secsSince1900 - seventyYears;

  return epoch;
}

void fetchSchedule(char *responseOut) {
  const char sha1Fingerprint[] = "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33";
	const int httpsPort = 443;
	const char* host = "raw.githubusercontent.com";
  const String url = String("/davidhampgonsalves/IR-Schedule-Thermostat/master/schedules/") + NODE_ID + ".json";

	WiFiClientSecure client;
  if (!client.connect(host, httpsPort))
    Serial.println("connection failed");

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: Mozilla/5.0 (iPhone; U; CPU iPhone OS 4_3_3 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8J2 Safari/6533.18.5\r\n" +
               "Connection: close\r\n\r\n");

	String response = client.readString();
	String json = response.substring(response.indexOf("[") - 1);
	json.toCharArray(responseOut, json.length());
}

void transmitScheduleSettings(JsonArray& change) {
  // My heatpump is Gree YAA
  uint8_t states[][8] = {
    {0x0C, 0x00, 0x60, 0x50, 0x00, 0x40, 0x00, 0xA0}, // heat mode, 16 degrees
    {0x0C, 0x01, 0x60, 0x50, 0x00, 0x40, 0x00, 0xB0},
    {0x0C, 0x02, 0x60, 0x50, 0x00, 0x40, 0x00, 0xC0},
    {0x0C, 0x03, 0x60, 0x50, 0x00, 0x40, 0x00, 0xD0},
    {0x0C, 0x04, 0x60, 0x50, 0x00, 0x40, 0x00, 0xE0},
    {0x0C, 0x05, 0x60, 0x50, 0x00, 0x40, 0x00, 0xF0},
    {0x0C, 0x06, 0x60, 0x50, 0x00, 0x40, 0x00, 0x00},
  };

	uint8_t offState[8] = {0x44, 0x05, 0x20, 0x50, 0x01, 0x40, 0x00, 0x70};

  IRsend irsend(LED_PIN);
  irsend.begin();

	if(change.get<int>(POWER) == false)
		irsend.sendGree(offState);
	else
		irsend.sendGree(states[change.get<int>(TEMP) - 16]);

  Serial.print("Sending IR command to A/C: ");
	Serial.println(change.get<int>(TEMP));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(ESP.getResetReason());
  Serial.println(ESP.getResetInfoPtr()->reason);
  system_rtc_mem_read(65, &state, sizeof(state));

  // init if not deep sleep wake
  if(ESP.getResetInfoPtr()->reason != 5) {
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, pass);
		while (WiFi.status() != WL_CONNECTED) { delay(100); }

    fetchSchedule(state.schedule);

    unsigned long currentTime = fetchNTPTime();
    setTime(currentTime);
    adjustTime(60 * 60 * -4); // apply TZ (but not DST)

    state.counter = 0;
  } else
     setTime(state.lastTime + ONE_HOUR_IN_SECONDS); // this is inacurate and so is the "rtc" of the ESP but close enough

  Serial.print("schedule: ");
  Serial.println(state.schedule);

  StaticJsonBuffer<500> jsonBuffer;
  JsonArray& schedule = jsonBuffer.parseArray(state.schedule);
  if (!schedule.success()) {
    Serial.print("schedule json could not be parsed: `");
    Serial.print(state.schedule);
    Serial.println("`.");
  }

  const int scheduleChangeCount = schedule.size();
  unsigned int scheduleIndex;
  for(int i=0 ; i < scheduleChangeCount ; i++) {
    const int startHour = schedule[i][HOUR];
    const int startMinute = schedule[i][MINUTE];
    if(hour() >= startHour && minute() >= startMinute) {
      scheduleIndex = i;
    } else
      break;
  }

  if(state.lastScheduleIndex != scheduleIndex)
    transmitScheduleSettings(schedule[scheduleIndex]);
  else
    Serial.println("skipping IR update, current settings are the same as last run");

  Serial.print("sleeping, count = ");
  Serial.println(state.counter);
  state.lastScheduleIndex = scheduleIndex;
  state.counter += 1;
  state.lastTime = now();
  system_rtc_mem_write(65, &state, sizeof(state));
  ESP.deepSleep(ONE_HOUR_IN_MICRO); // sleep for 1 hour
}

void loop() { }


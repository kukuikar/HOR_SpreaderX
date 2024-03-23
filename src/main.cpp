#include <GyverMotor2.h>
#include <GParser.h>
#include <Servo.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <EEPROM.h>

AsyncWebServer server(80);

//#define MKD_Guest
#ifdef MKD_Guest
const char *ssid = "MKD-Guest";
const char *password = "123Qweasd";
#else
const char *ssid = "MKD_HORIZON";
const char *password = "123Qweasd";
#endif
const uint32_t port = 1234;
const char *hostname = "SPREADER";
WiFiUDP udp;

IPAddress ServIP(192, 0, 0, 0);

const int8_t Rotate_DIR = 13;
const int8_t Rotate_PWM = 12;

const int8_t Telescop_DIR = 4;
const int8_t Telescop_PWM = 5;

GMotor2<DRIVER2WIRE> MOT_Rotate(Rotate_DIR, Rotate_PWM);
GMotor2<DRIVER2WIRE> MOT_Telescop(Telescop_DIR, Telescop_PWM);

Servo twistlocks;
int angle=180;

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Started: ");
  Serial.println(WiFi.localIP());
  udp.begin(port);

  twistlocks.attach(14); //

  MOT_Rotate.setMinDuty(100); // мин. ШИМ
  MOT_Rotate.reverse(1);      // реверс
  MOT_Rotate.setDeadtime(1);  // deadtime

  MOT_Telescop.setMinDuty(100); // мин. ШИМ
  MOT_Telescop.reverse(1);      // реверс
  MOT_Telescop.setDeadtime(1);  // deadtime

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", hostname); });

  AsyncElegantOTA.begin(&server); // Start AsyncElegantOTA
  server.begin();
}

void loop()
{
  MOT_Rotate.tick();
  MOT_Telescop.tick();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  static uint32_t tmr0 = millis();
  if (millis() - tmr0 > 25)
  {
    tmr0 = millis();
    int psize = udp.parsePacket();
    if (psize > 0)
    {
      char buf[32];
      int len = udp.read(buf, 32);
      IPAddress newIP;

      buf[len] = '\0'; // Парсим
      GParser data(buf);
      int ints[data.amount()];
      data.parseInts(ints);
      Serial.println(buf);
      
      if (newIP.fromString(buf))
      {
        if (strcmp(newIP.toString().c_str(), ServIP.toString().c_str()))
        {
          Serial.print("Server IP changed: ");
          Serial.print(ServIP);
          Serial.print(" -- >> ");
          ServIP = newIP;
        }
        Serial.println(ServIP);
      }
      else if (buf[0] == '1')
      {

        int Rotate_speed = ints[2] == 127 ? 0 : map(ints[2], 0, 255, -255, 255);
        int Telescope_speed = ints[3] == 127 ? 0 : map(ints[3], 0, 255, -255, 255);

        Serial.println(Rotate_speed);
        Serial.println(Telescope_speed);

        MOT_Rotate.setSpeed(Rotate_speed);
        MOT_Telescop.setSpeed(Telescope_speed);
        if (ints[4]==180){
          ints[4]=175;
        }else if(ints[4]==0){
          ints[4]=5;
        }
        twistlocks.write(ints[4]);
        /*
        if (ints[4]==180){
          while (angle<180){
            twistlocks.write(angle);
            angle=angle+8;
            delay(1);
          }
          twistlocks.write(angle);
        }else if (ints[4]==0){
          while (angle>0){
            twistlocks.write(angle);
            angle=angle-8;
            delay(1);
          }
          twistlocks.write(angle);
        }*/
      }
    }
  }

  static uint32_t tmr = millis();
  if (millis() - tmr > 25)
  {
    tmr = millis();
    char buf[32] = "_";
    strncat(buf, hostname, strlen(hostname));
    udp.beginPacket(ServIP, port);
    udp.printf(buf);
    udp.endPacket();
  }
}
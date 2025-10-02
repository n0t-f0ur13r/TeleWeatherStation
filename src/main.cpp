#include <Arduino.h>

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "TinyGPS++.h"

#define UPDATING_PERIOD_MS 5000

// GPS SETUP
#define RXD2 16
#define TXD2 17
#define GPS_BAUD_RATE 9600

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// WIFI SETUP
char ssid[] = "dd-wrt";
char pass[] = "5iMLKcnkLk2MGc";

char servername[] = "api.thingspeak.com";
WiFiClientSecure client;

const char *test_root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
    "MrY=\n"
    "-----END CERTIFICATE-----\n";


// DATA Variables

unsigned int humidity = 0;
double temperature = 0;
double atm_pressure = 0;

double latitude = 0;
double longitude = 0;

uint32_t sat_amount = 0;
int raining = 0;

void sendDataToApi(unsigned int humidity, double temperature, double atm_pressure, double latitude, double longitude, uint32_t sat_amount, int raining);

void setup()
{
  randomSeed(analogRead(0));

  Serial.begin(115200);
  Serial.println("# Attempting to connect to wireless AP...");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("- WiFi connection not yet stablished...");
    delay(200);
  }
  Serial.println("+ WiFi connetion OK.");
  Serial.print(" IP Address: ");
  Serial.println(WiFi.localIP());

  client.setCACert(test_root_ca);

  Serial.println("\nStarting connection to server...");
  if (!client.connect(servername, 443))
    Serial.println("Connection failed!");
  else
  {
    Serial.println("Connected to server!");

    // Make a HTTP request:
    client.print("GET ");
    client.print("/update?api_key=");
    client.print(MYKEY);
    client.print("&field1=");
    client.print(random(100));
    client.println(" HTTP/1.1");

    client.print("Host: ");
    client.println(servername);

    client.println("Connection: close");
    client.println();

    while (client.connected())
    {
      String line = client.readStringUntil('\n');
      if (line == "\r")
      {
        Serial.println("headers received");
        break;
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available())
    {
      char c = client.read();
      Serial.write(c);
    }

    client.stop();
  }

  Serial.println("# Attempting to begin GPS communication...");

  gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, RXD2, TXD2);

  Serial.println(" + GPS serial communication started.");
  delay(10000);
}

void loop()
{
  unsigned long start = millis();

  while (millis() - start < 1000)
  {
    while (gpsSerial.available() > 0)
    {
      gps.encode(gpsSerial.read());
    }
    if (gps.hdop.isUpdated())
    {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      sat_amount = gps.satellites.value();
    }
  }

  sendDataToApi(100, 101, 102, latitude, longitude, sat_amount, 0);

  delay(UPDATING_PERIOD_MS);
}

void sendDataToApi(unsigned int humidity, double temperature, double atm_pressure, double latitude, double longitude, uint32_t sat_amount, int raining)
{
  if (!client.connect(servername, 443))
  {
    Serial.println("Connection to ThingsPeak failed!");
  }
  else
  {
    Serial.println("Connected to server\nSending data...");

    client.print("GET ");
    client.print("/update?api_key=");
    client.print(MYKEY);

    client.print("&field1=");
    client.print(humidity);

    client.print("&field2=");
    client.print(temperature);

    client.print("&field3=");
    client.print(atm_pressure);

    client.print("&field4=");
    client.print(raining);

    client.print("&field5=");
    client.print(latitude);

    client.print("&field6=");
    client.print(longitude);

    client.print("&field7=");
    client.print(sat_amount);

    client.println(" HTTP/1.1");

    client.print("Host: ");
    client.println(servername);

    client.println("Connection: close");
    client.println();

    client.flush();
    client.stop();
  }
}

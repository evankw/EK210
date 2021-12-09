#include "WiFiEsp.h"
#include <ArduinoJson.h>
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
 

SoftwareSerial Serial1(10,11);
#endif
#define ESP8266_BAUD 9600
 
// If you're copying this code: change the ssid and password to match your settings!
char ssid[] = "BU Guest (unencrypted)";
char pass[] = "";
int status = WL_IDLE_STATUS;
 
WiFiEspServer server(80);
 
int trigPin = 8;
int echoPin = 9;
long duration, cm;
 
int baseline;
int previous;
boolean flood = false;
 
int intDistance()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    pinMode(echoPin, INPUT);
    duration = pulseIn(echoPin, HIGH);
    int cm = (duration/2) / 29.1;
    return cm;
}
 
int getDistance()
{
    int avg = intDistance();
    delay(1000);
    avg += intDistance();
    delay(1000);
    avg += intDistance();
    int cm = avg / 3;
    return baseline-cm;
}
 
boolean isFlooding(int distance)
{
    return  distance < (baseline - 2);
}
 
void setup()
{
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);    
    Serial.begin(9600);
    Serial1.begin(9600);
    WiFi.init(&Serial1);
    while (status != WL_CONNECTED)
    {
        Serial.print("Connecting to wifi network: ");
        Serial.println(ssid);
 
        status = WiFi.begin(ssid, pass);
    }
    Serial.print("IP Address of ESP8266 Module is: ");
    Serial.println(WiFi.localIP());
    Serial.println("You're connected to the network");
    server.begin();
    baseline = getDistance();
    flood = false;
    previous = baseline;
}
 
void loop()
{
    WiFiEspClient client = server.available();
    if (client)
    {
        String json = "";
        Serial.println("A client has connected");
 
        while (client.connected())
        {
            if ((client.available()) or (flood))
            {
                client.readStringUntil('{');
                String jsonStrWithoutBrackets = client.readStringUntil('}');
                String jsonStr = "{" + jsonStrWithoutBrackets + "}";
                if ((jsonStr.indexOf('{', 0) >= 0) or (flood))
                {
                    const size_t bufferSize = JSON_OBJECT_SIZE(1) + 20;
                    DynamicJsonBuffer jsonBuffer(bufferSize);
                    JsonObject &root = jsonBuffer.parseObject(jsonStr);
                    const char *value = root["action"];
                    Serial.println(value);
                    int dist = getDistance();
                    String distance = (String) dist;
                    // I know these do nothing, I meant to use these but got stuck troubleshooting sending messages in the first place.
                    boolean isf = isFlooding(dist);
                    String isFlood = String (isf);
                    if ((strcmp(value, "on") == 0) or flood)
                    {
                        Serial.println("Received on command");
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Distance: " + distance + "\r\n"
                            "\r\n"
                        );
                        client.stop();
                    }
                    else if (strcmp(value, "reset") == 0)
                    {
                        Serial.println("Received reset command");
                        baseline = intDistance();
                        String base = (String) baseline;
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Baseline: " + base + "\r\n"
                            "\r\n"
                        );
                        client.stop();
                    }
                    else if (strcmp(value, "base") == 0)
                    {
                        String base = String (baseline);
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Distance: " + base + "\r\n"
                            "\r\n"
                        );
                        client.stop();
                    }
                    else if (strcmp(value, "previous") == 0)
                    {
                        String prev = String (previous);
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Previous: " + prev + "\r\n"
                            "\r\n"
                        );
                        client.stop();
                    }
                    else if (strcmp(value, "off") == 0)
                    {
                        Serial.println("Received off command");
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Connection: close\r\n"
                            "\r\n");
                        client.stop();
                    }
                }
                else
                {
                    client.print(
                        "HTTP/1.1 500 ERROR\r\n"
                        "Connection: close\r\n"
                        "\r\n");
 
                    Serial.println("Error, bad or missing json");
                    client.stop();
                }
            }
        }
        delay(10);
        client.stop();
        Serial.println("Client disconnected");
    }
}

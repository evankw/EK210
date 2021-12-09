/* ESP Server receive and parse JSON
 
*/
#include "WiFiEsp.h"
#include <ArduinoJson.h>
#include <Adafruit_SleepyDog.h>
 
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
 
// set up software serial to allow serial communication to our TX and RX pins
SoftwareSerial Serial1(10,11);
#endif
 
// Set  baud rate of so we can monitor output from esp.
#define ESP8266_BAUD 9600
 
// CHANGE THIS TO MATCH YOUR SETTINGS
char ssid[] = "BU Guest (unencrypted)";
char pass[] = "";
int status = WL_IDLE_STATUS;
 
// Define an esp server that will listen on port 80
WiFiEspServer server(80);
 
int trigPin = 8;    // Trigger
int echoPin = 9;    // Echo
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
 
  // Convert the time into a distance
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
    // Open up communications for arduino serial and esp serial at same rate
    Serial.begin(9600);
    Serial1.begin(9600);
 
    // Initialize the esp module
    WiFi.init(&Serial1);
 
    // Start connecting to wifi network and wait for connection to complete
    while (status != WL_CONNECTED)
    {
        Serial.print("Conecting to wifi network: ");
        Serial.println(ssid);
 
        status = WiFi.begin(ssid, pass);
    }
 
    // Once we are connected log the IP address of the ESP module
    Serial.print("IP Address of ESP8266 Module is: ");
    Serial.println(WiFi.localIP());
    Serial.println("You're connected to the network");
 
    // Start the server
    server.begin();
    baseline = getDistance();
    flood = false;
    previous = baseline;
}
 
 
//  Continually check for new clients
void loop()
{
    WiFiEspClient client = server.available();
 
    // If a client has connected...
    if (client)
    {
        String json = "";
        Serial.println("A client has connected");
 
        while (client.connected())
        {
            // Read in json from connected client
            if ((client.available()) or (flood))
            {
                // ignore headers and read to first json bracket
                client.readStringUntil('{');
 
                // get json body (everything inside of the main brackets)
                String jsonStrWithoutBrackets = client.readStringUntil('}');
 
                // Append brackets to make the string parseable as json
                String jsonStr = "{" + jsonStrWithoutBrackets + "}";
 
                // if we managed to properly form jsonStr...
                if ((jsonStr.indexOf('{', 0) >= 0) or (flood))
                {
                    // parse string into json, bufferSize calculated by https://arduinojson.org/v5/assistant/
                    const size_t bufferSize = JSON_OBJECT_SIZE(1) + 20;
                    DynamicJsonBuffer jsonBuffer(bufferSize);
 
                    JsonObject &root = jsonBuffer.parseObject(jsonStr);
 
                    // get and print the value of the action key in our json object
                    const char *value = root["action"];
                    Serial.println(value);
                    int dist = getDistance();
                    String distance = (String) dist;
                    boolean isf = isFlooding(dist);
                    String isFlood = String (isf);
                    if ((strcmp(value, "on") == 0) or flood)
                    {
                        // Do something when we receive the on command
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
                        // Do something when we receive the off command
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
                    // send response and close connection
                    else if (strcmp(value, "off") == 0)
                    {
                        Serial.println("Received off command");
                        client.print(
                            "HTTP/1.1 200 OK\r\n"
                            "Connection: close\r\n" // the connection will be closed after completion of the response
                            "\r\n");
                        client.stop();
                    }
                }
                else
                {
                    // we were unable to parse json, send http error status and close connection
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

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Soladin.h>
#include "main.h"

#define RXD2 16
#define TXD2 17

// #define DEBUG // When DEBUG is defined log text will be sent to the UART programming port.

#ifdef DEBUG
    #define TRACE(x) Serial.print(x);
#else
    #define TRACE(x)
#endif

#define SLEEP_TIME 10000000  				//ESP32 will sleep for 10 seconds

/* Put your SSID & Password */
const char *ssid = "ssid_goes_here";        // Enter SSID here
const char *password = "password"; 			//Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 0, 10);		// Device IP address
IPAddress gateway(192, 168, 0, 1);			// Device Gateway
IPAddress subnet(255, 255, 255, 0);			// Device Subnet
IPAddress primaryDNS(8, 8, 8, 8);    		// Optional
IPAddress secondaryDNS(8, 8, 4, 4);  		// Optional

// mqtt client

const char *mqtt_server = "192.168.0.2";	// MQTT Server Address here
const char *mqtt_user = "user_goes_here";	// MQTT Username here
const char *mqtt_password = "password";		// MQTT Password here

WiFiClient espClient;
PubSubClient client(espClient);

// soladin
Soladin sol ;						 		// instance of soladin class
boolean solconnected = false ;			    // soladin connection status

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        #ifdef DEBUG
            TRACE("Attempting MQTT connection...\n");
        #endif
        // Attempt to connect
        if (client.connect("SolarInverterClient", mqtt_user, mqtt_password)) {
            #ifdef DEBUG
                TRACE("connected\n");
            #endif
        }
        else {
            #ifdef DEBUG
                TRACE("failed, rc=");
                TRACE(client.state());
                TRACE(" try again in 5 seconds\n");
            #endif
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void uploaddata(void) {
    if (solconnected && client.connected()) {
        if (sol.query(DVS)) {				// request Device status
            TRACE("Getting Device Status\n");
            if (sol.Flag != 0x00) {				// Print error flags
                TRACE("Error flags found\n");
                client.publish("/home/solarpv/alarmflag", String(sol.Flag).c_str(), true);
            }
            else{
                TRACE("Sending MQTT Messages\n");
                // upload data to the mqtt client
                client.publish("/home/solarpv/pvvolt", String(float(sol.PVvolt)/10).c_str(), true);
                client.publish("/home/solarpv/pvamp", String(float(sol.PVamp)/100).c_str(), true);
                client.publish("/home/solarpv/gridvolt", String(sol.Gridvolt).c_str(), true);
                client.publish("/home/solarpv/gridpower", String(sol.Gridpower).c_str(), true);
                client.publish("/home/solarpv/gridfrequency", String(float(sol.Gridfreq)/100).c_str(), true);
                client.publish("/home/solarpv/totalpower", String(float(sol.Totalpower)/100).c_str(), true);
                client.publish("/home/solarpv/devicetemp", String(sol.DeviceTemp).c_str(), true);
                char timeStr[14];
                sprintf(timeStr, "%04d:",(sol.TotalOperaTime/60));
                client.publish("/home/solarpv/operationtime", String(timeStr).c_str(), true);
                client.publish("/home/solarpv/alarmflag", String(sol.Flag).c_str(), true);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    //Set sleep timer
	esp_sleep_enable_timer_wakeup(SLEEP_TIME);

    // Connect to WiFi
    TRACE("\nConnecting to Wifi\n");
    WiFi.mode(WIFI_STA);

    if (!WiFi.config(local_ip, gateway, subnet, primaryDNS, secondaryDNS))
    {
        TRACE("Wifi Failed to configure\n");
    }

    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        TRACE(".");
    }
    TRACE("\n");
    TRACE("Connected to ");
    TRACE(ssid);
    TRACE("\nIP address: ");
    TRACE(WiFi.localIP());
    TRACE("\nMAC address: ");
    TRACE(WiFi.macAddress());
    TRACE("\n");

    // set up mqtt client

	client.setServer(mqtt_server, 1883);

    // set up soladin
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    sol.begin(&Serial2);

    // soladin connect loop
    while (!solconnected) {      					// Try to connect
        TRACE("Cmd: Probe");
        for (int i=0 ; i < 4 ; i++) {
        if (sol.query(PRB)) {				// Try connecting to slave
            solconnected = true;
            TRACE("...Connected");
            break;
        }
        TRACE(".");
        delay(1000);
        }
    }
}

void loop() {
    if (!client.connected()) {
		reconnect();
	}
	client.loop();

    uploaddata();

    //Go to sleep now
	esp_deep_sleep_start();
}
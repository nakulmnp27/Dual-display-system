#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "vivo 1920";
const char* password = "mnpdevil7";

const char* slaveHostName = "esp-slave";

WebServer server(80);
Preferences preferences;

String receivedMessage = "";

void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.display();

    preferences.begin("oled-messages", false);
    receivedMessage = preferences.getString("lastMessage", "No message received");
    displayMessage(receivedMessage);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("(SLAVE) Connecting to WiFi...");
    }
    Serial.println("(SLAVE) Connected to WiFi");
    Serial.print("(SLAVE) IP Address: ");
    Serial.println(WiFi.localIP());

    if (!MDNS.begin(slaveHostName)) {
        Serial.println("(SLAVE) Error starting mDNS");
    }

    server.on("/receive", HTTP_GET, handleReceive);
    server.begin();
}

void loop() {
    server.handleClient();
}

void handleReceive() {
    if (server.hasArg("message")) {
        receivedMessage = server.arg("message");
        preferences.putString("lastMessage", receivedMessage);
        displayMessage(receivedMessage);
        server.send(200, "text/plain", "Message received and displayed");
    } else {
        server.send(400, "text/plain", "No message received");
    }
}

void displayMessage(String message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Slave OLED:");
    display.setCursor(0, 20);
    display.print(message);
    display.display();
}

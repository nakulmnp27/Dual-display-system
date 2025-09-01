#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
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

String currentMessage = "";
bool isLoggedIn = false;

const char* loginPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Login - ESP32 OLED Message Sender</title>
<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #e9ecef; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
.container { background-color: white; padding: 40px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); text-align: center; }
h1 { color: #343a40; font-size: 24px; margin-bottom: 20px; }
input[type="password"] { padding: 12px; margin: 20px 0; width: 100%; max-width: 300px; border: 1px solid #ced4da; border-radius: 5px; font-size: 16px; }
input[type="submit"] { padding: 12px 25px; background-color: #007BFF; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; max-width: 300px; transition: background-color 0.3s; }
input[type="submit"]:hover { background-color: #0056b3; }
p, a { color: #6c757d; text-decoration: none; transition: color 0.3s; }
a:hover { color: #007BFF; }
</style>
</head>
<body>
<div class="container">
<h1>Login</h1>
<form action="/login" method="POST">
<input type="password" name="password" placeholder="Enter password" required><br>
<input type="submit" value="Login">
</form>
<p><a href="/">Go Back</a></p>
</div>
</body>
</html>
)rawliteral";

const char* mainPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 OLED Message Sender</title>
<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f8f9fa; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
.container { background-color: white; padding: 40px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); text-align: center; max-width: 500px; width: 100%; }
h1 { color: #343a40; font-size: 24px; margin-bottom: 20px; }
input[type="text"], select { padding: 12px; margin: 20px 0; width: 100%; max-width: 400px; border: 1px solid #ced4da; border-radius: 5px; font-size: 16px; }
input[type="submit"] { padding: 15px 30px; background-color: #28a745; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 18px; width: 100%; max-width: 400px; transition: background-color 0.3s; }
input[type="submit"]:hover { background-color: #218838; }
</style>
</head>
<body>
<div class="container">
<h1>ESP32 OLED Message Sender</h1>
<form action="/send" method="POST">
<input type="text" name="message" placeholder="Enter message" required><br>
<select name="target">
<option value="both">Send to Both (Master & Slave)</option>
<option value="master">Send to Only Master</option>
<option value="slave">Send to Only Slave</option>
</select><br>
<input type="submit" value="Send Message">
</form>
</div>
</body>
</html>
)rawliteral";

const char* successPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Message Sent</title>
<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f8f9fa; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
.container { background-color: white; padding: 40px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); text-align: center; }
h1 { color: #28a745; font-size: 24px; }
p { font-size: 16px; color: #343a40; }
.button-group { margin-top: 20px; }
.button-group a { padding: 10px 20px; text-decoration: none; border-radius: 5px; margin: 5px; display: inline-block; color: white; }
.send-message { background-color: #28a745; }
.logout { background-color: #dc3545; }
.button-group a:hover { opacity: 0.9; }
</style>
</head>
<body>
<div class="container">
<h1>Message Sent Successfully!</h1>
<p>Your message has been sent.</p>
<div class="button-group">
<a href="/" class="send-message">Send Another Message</a>
<a href="/logout" class="logout">Log Out</a>
</div>
</div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay();
  display.display();

  preferences.begin("oled-messages", false);
  currentMessage = preferences.getString("lastMessage", "No message yet");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  displayMessage("IP: " + WiFi.localIP().toString());
  delay(7000);
  displayMessage(currentMessage);

  if (!MDNS.begin("esp-master")) Serial.println("Error starting mDNS");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/logout", HTTP_GET, handleLogout);
  server.begin();
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  server.send(200, "text/html", isLoggedIn ? mainPage : loginPage);
}

void handleLogin() {
  String password = server.arg("password");
  if (password == "1234") {
    isLoggedIn = true;
    server.send(200, "text/html", mainPage);
  } else {
    server.send(403, "text/html", "<h1>Forbidden</h1><p>Incorrect Password!</p><a href=\"/\">Go Back</a>");
  }
}

void handleSend() {
  String message = server.arg("message");
  String target = server.arg("target");

  if (target == "master" || target == "both") {
    currentMessage = message;
    preferences.putString("lastMessage", currentMessage);
    displayMessage(currentMessage);
  }
  if (target == "slave" || target == "both") sendMessageToSlave(message);

  server.send(200, "text/html", successPage);
}

void handleLogout() {
  isLoggedIn = false;
  server.send(200, "text/html", loginPage);
}

void displayMessage(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Master OLED:");
  display.setCursor(0,20);
  display.print(message);
  display.display();
}

void sendMessageToSlave(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String encodedMessage = urlEncode(message);
    String url = "http://" + String(slaveHostName) + ".local/receive?message=" + encodedMessage;
    http.begin(url);
    int code = http.GET();
    if (code > 0) Serial.println("Message sent to Slave: " + http.getString());
    else Serial.println("Error sending to Slave: " + String(code));
    http.end();
  }
}

String urlEncode(String str) {
  String encoded = "";
  char buf[4];
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) encoded += c;
    else if (c == ' ') encoded += "%20";
    else { sprintf(buf, "%%%02X", c); encoded += buf; }
  }
  return encoded;
}

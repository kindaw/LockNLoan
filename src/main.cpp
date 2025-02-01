#include <Arduino.h>
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>

#define BUZZER 19
#define RX2_PIN 16
#define TX2_PIN 17
#define SCREEN_SCL 21
#define SCREEN_SDA 22
#define Button_Red 5
#define Button_Blue 18
#define relaypin 27

// telegramBot Setup
const String Bot_Token = "";
const String Chat_ID = "";
HTTPClient http;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE, SCREEN_SCL, SCREEN_SDA);
HardwareSerial GM865(2);
WiFiManager wifiManager;
String currentData = "";
void drawWiFiSignal(int rssi)
{
  // Map RSSI to a signal strength level (0-4)
  int level = 0;
  if (rssi > -50)
    level = 4; // Excellent
  else if (rssi > -60)
    level = 3; // Good
  else if (rssi > -70)
    level = 2; // Fair
  else if (rssi > -80)
    level = 1; // Weak
  else
    level = 0; // No signal

  // Draw the WiFi signal icon based on the level
  u8g2.setDrawColor(1);     // Set color to white
  int startX = 128 - 6 * 5; // Calculate starting X position for the icon (5 bars, each 6 pixels wide)
  for (int i = 0; i < 5; i++)
  {
    if (i < level)
    {
      u8g2.drawBox(startX + i * 6, 0, 4, 10); // Draw filled box for signal level
    }
    else
    {
      u8g2.drawFrame(startX + i * 6, 0, 4, 10); // Draw frame for no signal
    }
  }
}

void updateScreen(const char *line1, const char *line2 = "") {
  u8g2.clearBuffer();
  drawWiFiSignal(WiFi.RSSI()); // Draw the WiFi signal strength

  // Adjust the Y-coordinate to move the text below the WiFi icon
  u8g2.setCursor(0, 20); // Start below the WiFi icon
  u8g2.print(line1);

  if (strlen(line2) > 0) {
    // Check if line2 exceeds screen width and split if necessary
    String line2Str = String(line2);
    int maxWidth = 16; // Assuming 16 characters per line for the screen width

    if (line2Str.length() > maxWidth) {
      u8g2.setCursor(0, 30);
      u8g2.print(line2Str.substring(0, maxWidth));
      u8g2.setCursor(0, 45);
      u8g2.print(line2Str.substring(maxWidth));
    } else {
      u8g2.setCursor(0, 30);
      u8g2.print(line2Str);
    }
  }

  u8g2.sendBuffer();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  updateScreen("AP Mode:", myWiFiManager->getConfigPortalSSID().c_str());
}

void startbeep()
{
  tone(BUZZER, 1600);
  delay(100);
  tone(BUZZER, 1800);
  delay(100);
  tone(BUZZER, 2200);
  delay(100);
  noTone(BUZZER);
}

String urlEncode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      encodedString += '%';
      code0 = (c >> 4) & 0xF;
      code1 = c & 0xF;
      encodedString += (char)(code0 < 10 ? code0 + '0' : code0 - 10 + 'A');
      encodedString += (char)(code1 < 10 ? code1 + '0' : code1 - 10 + 'A');
    }
  }
  return encodedString;
}

void sentToTelegram(String message)
{
  String encodedMessage = urlEncode(message);
  String url = "https://api.telegram.org/bot" + Bot_Token + "/sendMessage?chat_id=" + Chat_ID + "&text=" + encodedMessage;

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0)
  {
    Serial.println("telegram message sent " + message);
    Serial.println(url);
  }
  else
  {
    Serial.println("error sending message " + String(httpCode));
  }
  http.end();
}

void handleButtons() {
  static unsigned long holdStartTime = 0; // For tracking button hold duration
  bool redPressed = digitalRead(Button_Red) == LOW;
  bool bluePressed = digitalRead(Button_Blue) == LOW;

  if (redPressed && bluePressed) {
    if (holdStartTime == 0) {
      holdStartTime = millis(); // Start hold timer
    }
    if (millis() - holdStartTime >= 3000) {
      currentData = ""; // Clear the data after holding both buttons for 3 seconds
      updateScreen("Data Cleared");
      Serial.println("Data cleared!");
      holdStartTime = 0; // Reset hold timer
    }
  } else {
    holdStartTime = 0; // Reset if buttons are released
    if (redPressed) {
      Serial.print("red");
      if (currentData.isEmpty()) {
        updateScreen("Need Data");
        Serial.println("No data available to send!");
      } else {
        String message = "loan form \n" + currentData;
        sentToTelegram(message);
        updateScreen("Sent Loan Form", currentData.c_str());
        Serial.println("Sent loan form "+ currentData +"s to Telegram.");
        digitalWrite(relaypin, LOW);
        delay(10000);
        digitalWrite(relaypin, HIGH);
        currentData = ""; //clear data after sent data
        updateScreen("ESP32 Ready", "Waiting for data...");
      }
      while (digitalRead(Button_Red) == LOW); // Wait until button is released
    } else if (bluePressed) {
      Serial.print("blue");
      if (currentData.isEmpty()) {
        updateScreen("Need Data");
        Serial.println("No data available to send!");
      } else {
        String message = "return form \n" + currentData;
        sentToTelegram(message);
        updateScreen("Sent Return Form", currentData.c_str());
        Serial.println("Sent return form "+ currentData +"to Telegram.");
        digitalWrite(relaypin, LOW);
        delay(10000);
        digitalWrite(relaypin, HIGH);
        currentData = ""; //clear data after sent data
        updateScreen("ESP32 Ready", "Waiting for data...");
      }
      while (digitalRead(Button_Blue) == LOW); // Wait until button is released
    }
  }
}


void setup()
{
  Serial.begin(115200);
  GM865.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  startbeep();
  // Initialize OLED
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  updateScreen("Connecting to WiFi...");

  // WiFi Setup
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect("ESP32_AP"))
  {
    updateScreen("Failed to connect", "Restarting...");
    delay(3000);
    ESP.restart();
  }

  String connMsg = "WiFi: " + WiFi.SSID();
  updateScreen(connMsg.c_str(), WiFi.localIP().toString().c_str());
  delay(2000);

  updateScreen("ESP32 Ready", "Waiting for data...");
  Serial.println("ESP32 UART with GM865 initialized. Waiting for data...");

  pinMode(Button_Blue, INPUT_PULLUP);
  pinMode(Button_Red, INPUT_PULLUP);
  pinMode(relaypin, OUTPUT);
  digitalWrite(relaypin, HIGH);
}

void loop()
{
  if (GM865.available())
  {
    currentData = GM865.readStringUntil('\n');
    Serial.println("Data from GM865: " + currentData);
    updateScreen("Data:", currentData.c_str());
  }

handleButtons();
  if (WiFi.status() != WL_CONNECTED)
  {
    updateScreen("WiFi Disconnected", "Reconnecting...");
    wifiManager.autoConnect("ESP32_AP");
  }

  delay(100);
}
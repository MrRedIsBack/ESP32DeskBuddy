// Standard Libraries

// For the I2C LCD screen
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// For the new SPI TFT Screen
#include <SPI.h>
#include <TFT_eSPI.h> 

// For Spotify and System
#include <Arduino.h>
#include <WiFi.h>
#include "SpotifyEsp32.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define DISABLE_SIMPLIFIED

#include "secrets.h"

// Pins for the joystick module
constexpr int xAxisPin = 34;
constexpr int yAxisPin = 35;
constexpr int buttonPin = 33;

// I2C Pins for the LCD
#define I2C_SDA 21
#define I2C_SCL 26

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Create instance of the new TFT screen
TFT_eSPI tft = TFT_eSPI(); 

// Create an instance of the Spotify class
Spotify sp(CLIENT_ID, CLIENT_SECRET, REFRESH_TOKEN);

void setup() {
  Serial.begin(115200);

  // 1. Initialize the new TFT Screen
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Loading...");

  // 2. Initialize the I2C LCD on the new pins
  Wire.begin(I2C_SDA, I2C_SCL); // THIS IS THE MAGIC LINE!
  lcd.init();
  lcd.backlight(); 
  lcd.setCursor(0, 0);
  lcd.print("Loading...");   
  delay(1000);

  pinMode(buttonPin, INPUT_PULLUP); 

  // 3. Connect to WiFi
  connectWifi();

  // Update screens after WiFi connects
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("WiFi Connected!");
  
  lcd.clear();
  lcd.print("Connected to WiFi");   
  delay(1000);

  // 4. Configure the JPEG Decoder for the TFT screen
  TJpgDec.setJpgScale(2); // Scales a 300x300 image down to 150x150
  TJpgDec.setSwapBytes(true); // Required so the colors don't look inverted on TFT_eSPI
  TJpgDec.setCallback(bitmap_callback);

  sp.begin();
}

// Add this variable right above your loop()
unsigned long lastSpotifyCheck = 0;
const unsigned long spotifyInterval = 5000; // 5 seconds (5000 milliseconds)

void loop() {
  // 1. READ JOYSTICK (Runs super fast, 10 times a second)
  analogRead(xAxisPin); 
  delay(5); 
  int xValue = analogRead(xAxisPin);

  analogRead(yAxisPin); 
  delay(5); 
  int yValue = analogRead(yAxisPin); 

  int btnValue = digitalRead(buttonPin);
  
  // 2. CHECK SPOTIFY (Runs only once every 5 seconds)
  if (millis() - lastSpotifyCheck >= spotifyInterval) {
    lastSpotifyCheck = millis(); // Reset the timer

    static String lastArtist;
    static String lastTrackname;
      
    // Now we are safely polling Spotify without spamming them!
    String currentArtist = sp.current_artist_names();
    String currentTrackname = sp.current_track_name();
    String currentImage = sp.get_current_album_image_url(1); 

    if (currentArtist.isEmpty() || currentArtist == "Something went wrong") {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error");
    }
    else if (lastTrackname != currentTrackname && currentTrackname != "null") {
      // The song changed! Update everything.
      fetchAndDisplayAlbumArt(currentImage);
      
      lastTrackname = currentTrackname;
      lastArtist = currentArtist;
      
      // Update LCD
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print(lastArtist); 
      lcd.setCursor(0, 0);
      lcd.print(lastTrackname);
      
      // Update TFT Track Info
      tft.fillRect(0, 200, 240, 40, TFT_BLACK); 
      tft.setTextSize(1);
      tft.setCursor(45, 205);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.println(lastTrackname);
      tft.setCursor(45, 220);
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.println(lastArtist);
    }
  }

  // A tiny delay just to keep the loop stable
  delay(50);
}

void connectWifi() {
  int attempts = 0;
  const int max_attempts = 20; 

  WiFi.begin(SSID, PASSWORD);
  Serial.print("\nConnecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nConnection timed out!");
    WiFi.disconnect();
    connectWifi();
  } else {
    Serial.println("\nConnected to WiFi!");
  }
}

// THIS IS ALL IT TAKES NOW! The decoder handles all the math.
bool bitmap_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Push the decoded block of pixels directly to the TFT screen
  tft.pushImage(x, y, w, h, bitmap);
  return true; 
}

void fetchAndDisplayAlbumArt(String url) {
  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  http.begin(client, url);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    uint8_t* imageBuffer = (uint8_t*) malloc(len);
    
    if (imageBuffer != nullptr) {
      WiFiClient *stream = http.getStreamPtr();
      int bytesRead = 0;
      
      // THE FIX: A robust loop that waits for the entire image to download!
      while (http.connected() && bytesRead < len) {
        size_t available = stream->available();
        if (available) {
          int bytesChunk = stream->readBytes(&imageBuffer[bytesRead], available);
          bytesRead += bytesChunk;
        }
        delay(1); // Keeps the ESP32 from crashing while waiting
      }
      
      // Only draw if we successfully got the whole image
      if (bytesRead == len) {
        tft.fillScreen(TFT_BLACK); // Clear the old screen
        TJpgDec.drawJpg(45, 45, imageBuffer, len); // Draw the new image
      } else {
        Serial.println("Download timed out or failed.");
      }
      
      free(imageBuffer);
    } else {
      Serial.println("Not enough RAM to hold the image!");
    }
  }
  http.end();
}
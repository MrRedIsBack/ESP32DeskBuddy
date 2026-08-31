// For the I2C LCD screen
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// For the SPI TFT Screen
#include <SPI.h>
#include <TFT_eSPI.h> 

// For Spotify
#include <Arduino.h>
#include <WiFi.h>
#include "SpotifyEsp32.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define DISABLE_SIMPLIFIED

// For Trading212
#include <ArduinoJson.h>

// For Nissan GTR R35
#include "nissan.h"

#include "secrets.h"

// User Config
const int trading_goal = 6000;

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

// --- STATE TRACKING VARIABLES ---
// All the different modes that we can loop through
unsigned int current_mode = 1; 
unsigned int previous_mode = 0; // Added to track when the mode actually changes
bool locked_mode = false; 
unsigned int number_of_modes = 4; 

unsigned long lastSpotifyCheck = 0;
const unsigned long spotifyInterval = 5000; 

// Moved these globally so we can reset them when switching menus
String lastArtist = "";
String lastTrackname = "";

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(buttonPin, INPUT_PULLUP); 

  // 1. Connect to WiFi FIRST
  connectWifi();

  // 2. Initialize Spotify
  //sp.set_log_level(SPOTIFY_LOG_VERBOSE);
  sp.begin();

  // 3. NOW Initialize the new TFT Screen
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("WiFi Connected!");

  // 4. Initialize the I2C LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight(); 
  lcd.setCursor(0, 0);
  lcd.print("Connected to"); 
  lcd.setCursor(0, 1);
  lcd.print("WiFi");
  delay(1000);

  // 5. Configure the JPEG Decoder for the TFT screen
  TJpgDec.setJpgScale(2); // Scales a 300x300 image down to 150x150
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(bitmap_callback);
}

void loop() {
  // 1. READ JOYSTICK
  analogRead(xAxisPin); 
  delay(5); 
  unsigned int xValue = analogRead(xAxisPin);

  analogRead(yAxisPin); 
  delay(5); 
  unsigned int yValue = analogRead(yAxisPin); 

  unsigned int btnValue = digitalRead(buttonPin);

  if (btnValue == 0) {
    locked_mode = !locked_mode; // Unlock/lock the menus
    while (btnValue == 0) { // Loop until we let go
      btnValue = digitalRead(buttonPin);
      delay(10);
    }
  }

  // Joystick Navigation
  if (xValue < 100 && locked_mode == false){
    if (current_mode == 1) {
      current_mode = number_of_modes; 
    } else {
      current_mode--;
    }
  }
  else if (xValue > 2400 && locked_mode == false){
    if (current_mode == number_of_modes) {
      current_mode = 1; 
    } else {
      current_mode++;
    }
  }

  // --- MODE 1: SPOTIFY ---
  if (current_mode == 1) {
    display_spotify();
  }

  else if (current_mode == 2) {
    if (current_mode != previous_mode) {
      display_storage();
    }
  }

  else if (current_mode == 3) {
    if (current_mode != previous_mode) {
      display_trading();
    }
  }

  else if (current_mode == 4) {
    if (current_mode != previous_mode) {
      lcd.clear();
      tft.setSwapBytes(true);
      tft.pushImage(0, 0, nissan_width, nissan_height, nissan);
    }
  }

  // Update previous_mode at the very end of the loop
  previous_mode = current_mode;
  
  delay(500);
}


// Helper Functions

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
    delay(3000); 
    Serial.println("\nConnected to WiFi!");
  }
}

bool bitmap_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
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
      
      while (http.connected() && bytesRead < len) {
        size_t available = stream->available();
        if (available) {
          int bytesChunk = stream->readBytes(&imageBuffer[bytesRead], available);
          bytesRead += bytesChunk;
        }
        delay(1); 
      }
      
      if (bytesRead == len) {
        tft.fillScreen(TFT_BLACK); 
        TJpgDec.drawJpg(45, 45, imageBuffer, len); 
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

void lcd_display(const String line1, const String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1); 
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void show_percentage(const String title, const int percentage_value) {
  // --- 1. TFT SCREEN UI ---
  tft.fillScreen(TFT_BLACK);
  
  // Draw Title at the top center
  tft.setTextDatum(TC_DATUM); // Top Center alignment
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(title, 120, 20); // 120 is the center of a standard 240px wide screen

  // Draw HUGE Percentage in the middle
  tft.setTextDatum(MC_DATUM); // Middle Center alignment
  
  // Make the text change color if memory gets dangerously low!
  if (percentage_value < 60) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  } else if (percentage_value < 85) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }
  
  tft.setTextSize(6); // Massive text size
  String percentStr = String(percentage_value) + "%";
  tft.drawString(percentStr, 120, 100); // Perfectly centered at x:120, y:100

  // Draw a visual progress bar near the bottom
  int barWidth = 200;
  int barHeight = 20;
  int barX = 20;
  int barY = 200;
  
  // Draw the white outline of the bar
  tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
  
  // Calculate how many pixels wide the filled part should be based on the percentage
  int fillWidth = (barWidth * percentage_value) / 100;
  
  // Fill the bar with the same color as the text
  tft.fillRect(barX, barY, fillWidth, barHeight, tft.textcolor); 

  // Reset text datum back to default top-left so it doesn't mess up your Spotify text later!
  tft.setTextDatum(TL_DATUM); 
}

// Modes

void display_spotify() {
  // If we JUST switched to Mode 1 from another mode
    if (current_mode != previous_mode) {
      lastTrackname = ""; // Force the screen to redraw the current song
      lastSpotifyCheck = millis() - spotifyInterval; // Force an immediate API check
    }

    if (millis() - lastSpotifyCheck >= spotifyInterval) {
      lastSpotifyCheck = millis(); // Reset the timer
      
      String currentArtist = sp.current_artist_names();
      String currentTrackname = sp.current_track_name();
      String currentImage = sp.get_current_album_image_url(1); 

      if (currentArtist.isEmpty() || currentArtist == "Something went wrong") {
        lcd_display("Spotify", "Error");

        tft.fillScreen(TFT_BLACK);
      }
      else if (lastTrackname != currentTrackname && currentTrackname != "null") {
        // The song changed! Update everything.
        fetchAndDisplayAlbumArt(currentImage);
        
        lastTrackname = currentTrackname;
        lastArtist = currentArtist;
        
        // Update LCD
        lcd_display(lastArtist, lastTrackname);
        
        // Update TFT Track Info
        tft.fillRect(0, 200, 240, 40, TFT_BLACK); 
        tft.setTextSize(1);
        tft.setCursor(10, 205);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println(lastTrackname);
        tft.setCursor(10, 220);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.println(lastArtist);
      }
    }
}

void display_storage() {
  uint32_t total_RAM = ESP.getHeapSize();
  uint32_t free_RAM = ESP.getFreeHeap();
  uint32_t used_RAM = total_RAM - free_RAM;

  // Calculate the percentage of RAM being used
  int used_Percentage = (used_RAM * 100) / total_RAM;

  lcd_display("Free Ram:", String(free_RAM/1000) + "KB");

  show_percentage("RAM IN USE", used_Percentage);

  // --- 2. SERIAL MONITOR ---
  Serial.println("--- ESP32 RAM Usage ---");
  Serial.print("Total RAM: ");
  Serial.print(total_RAM / 1000);
  Serial.println("K bytes");

  Serial.print("Used RAM:  ");
  Serial.print(used_RAM / 1000);
  Serial.println("K bytes");

  Serial.print("Free RAM:  ");
  Serial.print(free_RAM / 1000);
  Serial.println("K bytes");
}

void display_trading() {
  // 1. Setup a secure connection
  WiFiClientSecure client;
  client.setInsecure(); // Skips SSL certificate validation (fine for demo/testing)

  // 2. Initialize the HTTP Client with the endpoint URL
  HTTPClient http;
  http.begin(client, "https://live.trading212.com/api/v0/equity/account/summary");

  // 3. Apply Authentication
  Serial.println(TRADING212_KEY);
  http.addHeader("Authorization", TRADING212_KEY);

  // 4. Execute the GET request
  Serial.println("Sending request to Trading212...");
  int httpResponseCode = http.GET();

  // 5. Read the response
  if (httpResponseCode > 0) {
    Serial.print("HTTP Status Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 401) {
      Serial.println("Bad API key.");
    }
    else if (httpResponseCode == 403) {
      Serial.println("Scope( account ) missing for API key");
    }
    else if (httpResponseCode == 408) {
      Serial.println("Timed-out");
    }
    else if (httpResponseCode == 429) {
      Serial.println("Limited: 1 / 5s");
    }
    else if (httpResponseCode == 200) {
      // SUCCESS! Get the raw JSON string
      String payload = http.getString(); 
      
      // Parse the JSON
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("JSON Parsing failed: ");
        Serial.println(error.f_str());
      } else {
        // Extract the specific values you wanted
        float totalValue = doc["totalValue"];
        float profit = doc["investments"]["unrealizedProfitLoss"];

        int percentage_value = (totalValue/trading_goal)*100;

        lcd_display("Profit:", String(profit));

        show_percentage("Goal: " + String(trading_goal), percentage_value);

      }
    } else {
      Serial.println("Unexpected HTTP Status Code.");
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode));
  }

  // 6. Clean up resources
  http.end();
}
// Standard Libraries
#include <iostream>

// For the LCD screen
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// For the OLED screen
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// For Spotify
#include <Arduino.h>
#include <WiFi.h>
#include "SpotifyEsp32.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define DISABLE_SIMPLIFIED

// For WiFi
const char* SSID = "21BeauFlat";
const char* PASSWORD = "qaesnpmn";

// For Spotify
const char* CLIENT_ID = "5ca2cb1d9c354d3181d746fa43c7f703";
const char* CLIENT_SECRET = "8a82ccee9de248fb9e3959abea326b6d";
const char* REFRESH_TOKEN = "AQA-_CaADFGbVR3-Efn1GMPbcoK24hT8dqJESHj1mOEFNIPZnRi3hz2DxdhSMSL8nP63R9mi0gssO4R_oKyi5GDVzs3ROitjVx7mVURaukzTbpE0YK4AsTeOT1Gwn5xHMoM";

// Pins for the joystick module
constexpr int xAxisPin = 34;
constexpr int yAxisPin = 35;
constexpr int buttonPin = 33;

// For the OLED screen
constexpr int screeenWidth = 128;
constexpr int screenHeight = 64;

// The pins/addresses of the 2 screens
constexpr int OLED_address = 0x3c;
constexpr int LCD_address = 0x27;

//SDA->21,SCL->22 
//Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(LCD_address,16,2);

// Wire is Arduino's built-in object for handling I2C communication
// the -1 is for the reset pin
Adafruit_SSD1306 display(screeenWidth, screenHeight, &Wire, -1);

// Create an instance of the Spotify class
Spotify sp(CLIENT_ID, CLIENT_SECRET, REFRESH_TOKEN);

void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_address)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }

  // This is for the OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Loading...");
  display.display();

  // This is for the LCD
  lcd.init();
  lcd.backlight(); 
  lcd.print("Loading...");   
  delay(1000);

  pinMode(buttonPin, INPUT_PULLUP); 

  connectWifi();

  // This is for the OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Connected to");
  delay(100);
  display.setCursor(0, 1);
  display.println("WiFi!");
  display.display();

  // This is for the LCD
  lcd.init();
  lcd.backlight(); 
  lcd.print("Connected to WiFi");   
  delay(1000);

  TJpgDec.setJpgScale(1); 
  TJpgDec.setCallback(bitmap_callback);

  sp.begin();
}

void loop() {
  // Read X-axis: Dummy read, wait, real read
  analogRead(xAxisPin); 
  delay(5); // Gives the ESP32 ADC time to clear residual voltage
  int xValue = analogRead(xAxisPin);

  // Read Y-axis: Dummy read, wait, real read
  analogRead(yAxisPin); 
  delay(5); 
  int yValue = analogRead(yAxisPin); 

  // Read the digital value from the button pin
  int btnValue = digitalRead(buttonPin);
  
  // print out the values
  //Serial.printf("Joystick value is %d , %d , %d \n", xValue, yValue, btnValue);
  
  delay(100);  

  static String lastArtist;
  static String lastTrackname;
    
  String currentArtist = sp.current_artist_names();
  String currentTrackname = sp.current_track_name();
  String currentImage = sp.get_current_album_image_url(2);

  Serial.println(currentImage);

  if (currentArtist.isEmpty() || currentArtist == "Something went wrong") {
    lcd.clear();
    lcd.setCursor(0, 0); // Column 0, Row 0
    lcd.print("Error"); // If something is wrong, then display N/A
    delay(100);
  }

  if (lastTrackname != currentTrackname && currentTrackname != "Something went wrong" && currentTrackname != "null") {
    fetchAndDisplayAlbumArt(currentImage);
    lastTrackname = currentTrackname;
    lastArtist = currentArtist;
    lcd.clear();
    Serial.println("Artist: " + lastArtist);
    lcd.setCursor(0, 1); // Column 0, Row 1
    lcd.print(lastArtist); // Displays the artist
    delay(100);
    Serial.println("Track: " + lastTrackname);
    lcd.setCursor(0, 0); // Sets the cursor position to the first row and first column (0, 0).
    lcd.print(lastTrackname);
    delay(1000);
  }
}

void connectWifi() {
  int attempts = 0;
  const int max_attempts = 20; // 20 seconds timeout

  WiFi.begin(SSID, PASSWORD);
  Serial.print("\nConnecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  // Check if we broke out of the loop because of a timeout or a successful connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nConnection timed out!");
    
    // Stop the current connection attempt and reset the WiFi state
    WiFi.disconnect();
    connectWifi();
  } else {
    Serial.println("\nConnected to WiFi!");
  }
}

// A 4x4 Bayer matrix used to create the illusion of grey shades
const uint8_t bayer_matrix[4][4] = {
  { 15, 135,  45, 165 },
  { 195,  75, 225, 105 },
  {  60, 180,  30, 150 },
  { 240, 120, 210,  90 }
};

bool bitmap_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      int blockIndex = (row * w) + col;
      uint16_t color = bitmap[blockIndex];

      // Extract RGB values
      uint8_t r = (color & 0xF800) >> 8;
      uint8_t g = (color & 0x07E0) >> 3;
      uint8_t b = (color & 0x001F) << 3;

      // Calculate the true brightness of the pixel (0-255)
      // We use standard luminance weights (Green is brightest to the human eye)
      uint8_t brightness = (r * 0.299) + (g * 0.587) + (b * 0.114);

      // Find the absolute coordinate on the 64x64 image
      int pixel_x = x + col;
      int pixel_y = y + row;

      // Look up the threshold in our Bayer matrix based on the coordinates
      uint8_t threshold = bayer_matrix[pixel_y % 4][pixel_x % 4];

      // If the pixel's brightness is higher than the grid's threshold, make it white
      int oledColor = (brightness > threshold) ? WHITE : BLACK;

      // Draw it to the OLED! (Remember, we shifted it right by 32 pixels in drawJpg)
      display.drawPixel(pixel_x + 32, pixel_y, oledColor);
    }
  }
  return true; 
}

void fetchAndDisplayAlbumArt(String url) {
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation to save RAM/time
  
  HTTPClient http;
  http.begin(client, url);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    uint8_t* imageBuffer = (uint8_t*) malloc(len);
    
    if (imageBuffer != nullptr) {
      http.getStreamPtr()->readBytes(imageBuffer, len);
      
      display.clearDisplay(); // Clear the OLED screen
      
      // Draw the JPEG. We pass 32 for the X coordinate to center the 
      // 64x64 image perfectly in the middle of your 128x64 screen!
      TJpgDec.drawJpg(32, 0, imageBuffer, len); 
      
      display.display(); // Push the newly drawn buffer to the OLED
      free(imageBuffer);
    }
  }
  http.end();
}

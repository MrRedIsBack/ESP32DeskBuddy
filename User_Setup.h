#define USER_SETUP_INFO "User_Setup"
#define ST7789_DRIVER     // Tell it we are using the ST7789
#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define CGRAM_OFFSET      // Required for 1.3" screens

// The exact pins we are using
#define TFT_MOSI 23 // SDA
#define TFT_SCLK 18 // SCL
#define TFT_CS   -1 // Disables the CS pin requirement
#define TFT_DC   22
#define TFT_RST  4
#define TFT_BL   -1 // Backlight is hardwired

// Fonts and Speed
#define LOAD_GLCD
#define SPI_FREQUENCY  27000000
/**************************************************************************/
/*! 
    Datalogger sketch for the UniSA FSAE team.
    
*/
/**************************************************************************/

#define _CAT(a, ...) a ## __VA_ARGS__
#define SWITCH_ENABLED_0 0
#define SWITCH_ENABLED_1 1
#define SWITCH_ENABLED_  1
#define ENABLED(b) _CAT(SWITCH_ENABLED_, b)

#define NFC
#define GPS
#define DISPLAY
#define SDCARD 1  //1 for SD card in display, 2 for SD card in separate breakout
#define BUTTONS
#define EEPROM_ENABLED
#define ANALOG_INPUTS


#include <stdio.h>

//NFC Setup
#if ENABLED(NFC)
  //#include <Wire.h>
  #include <PN532_I2C.h>
  #include <PN532.h>
  //#include <NfcAdapter.h>

  PN532_I2C pn532i2c(Wire);
  PN532 nfc(pn532i2c);
#endif

//GPS SETUP
#if ENABLED(GPS)
  #include <TinyGPS++.h>
  
  static const uint32_t GPSBaud = 9600;
  TinyGPSPlus gps;  // The TinyGPS++ object
  
#endif

//Display SETUP
#if ENABLED(DISPLAY)
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1351.h>
  #include <SD.h>
  #include <SPI.h>
  
  //#define VERBOSE_DISPLAY
  #define SPLASH_SCREEN_ENABLED
  #define SPLASH_IMAGE "splash.bmp" //file to load from SD card. 
  #define SPLASH_DURATION 1000 //display duration in milliseconds
  
  //OLED Setup
  // You can use any (4 or) 5 pins 
  #define sclk 2
  #define mosi 3
  #define dc   49
  #define cs   47
  #define rst  48
  
  // Color definitions
  #define	BLACK           0x0000
  #define	BLUE            0x001F
  #define	RED             0xF800
  #define	GREEN           0x07E0
  #define CYAN            0x07FF
  #define MAGENTA         0xF81F
  #define YELLOW          0xFFE0  
  #define WHITE           0xFFFF
    
  // use the hardware SPI pins 
  Adafruit_SSD1351 tft = Adafruit_SSD1351(cs, dc, rst);
  
  String output;
  char char_buff[20];

#endif

//SD SETUP
#if ENABLED(SDCARD)
    #define SD_CS 43    

    // the file itself
    File bmpFile;
    
    // information we extract about the bitmap file
    int bmpWidth, bmpHeight;
    uint8_t bmpDepth, bmpImageoffset;

    const int LIMIT = 1000;
    char fileName[13];
    File dataFile;
#endif

//Button SETUP
#if ENABLED(BUTTONS)
  #include <Button.h>        //https://github.com/JChristensen/Button
  #define BUTTON_UP 36
  #define BUTTON_SEL 35
  #define BUTTON_SET 37

  #define PULLUP false
  #define INVERT false
  #define DEBOUNCE_MS 20
  #define LONG_PRESS 1000

  Button button_up(BUTTON_UP, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button
  Button button_sel(BUTTON_SEL, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button
  Button button_set(BUTTON_SET, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button

#endif

#if ENABLED(EEPROM_ENABLED)
  #include <EEPROM.h>

  int eeprom_address = 0;
  int eeprom_options_start_addr = 0;
#endif

#if ENABLED(ANALOG_INPUTS) 
  #define ANALOG1 A2
  #define ANALOG2 A3
  #define ANALOG3 A4
  #define ANALOG4 A5
  #define ANALOG5 A6
  #define ANALOG6 A7

  #define SAMPLE_FREQ 4 //every x'th loop sample and store values

  int analog1, analog2, analog3, analog4, analog5, analog6;
#endif

//timekeeping
#define LOOP_DELAY 20000 // Loop delay in us. This is the desired time between drive calls. Stability ensures accurate velocity calculations and motor control.

// Can put variables for checking accurate loop timing here, eg interupt time and scale factor.
int loop_number = 0; // variable to hold the loop number. Used to keep track of which topics to publish and when
unsigned long loop_time = micros(); // Loop time place holder for motor velocity calculation.
unsigned long last_loop_micros = micros();

int loop_index = 0;

//menu structure
#define MENU_OPTIONS 6
#define MENU_OPTION_HEIGHT 10
int menu_selected_index = 0;
bool menu_changed = false;
bool menu_changed_small = false;
bool menu_active = false;
bool menu_just_opened = false;

enum menu_items {
  GPS_ONOFF,
  LED_BRIGHTNESS,
  SD_LOGGING,
  INVERT_DISPLAY,
  CLEAR_EEPROM,
  NEW_LOGFILE
};

typedef struct menuOption {
  String optionName;
  int numStates;
  int curState;
  String stateText[5];
};

struct menuOption menuArray[MENU_OPTIONS];


//variables to format and display to driver

enum display_variables {
  V_SPEED,
  V_SPEED_AVG,
  V_RPM,
  V_VOLTS,
  V_TEMP,
  V_AMPS,
  V_POWER
};

enum variable_types {
  INT,
  FLOAT,
  BOOL,
  STRING
};

#define NUM_VARIABLES 4

typedef struct displayVariable {
  int type;
  int intVal;
  float floatVal;
  bool boolVal;
  String stringVal;
  String displaySymbol;
  String displayPrefix;
  String displaySuffix;
  int decPoints;
  bool valChanged;
  int symOffset;
};

struct displayVariable displayVariables[NUM_VARIABLES];

long randNumber;
int activeVariableBig = 0;
int activeVariableSmall1 = 0;
int activeVariableSmall2 = 0;
bool indicatorsJustChanged = false;

#define BATT_PIN A13
#define BATT_R1 10000
#define BATT_R2 4000

void setup(void) {
  Serial.begin(115200);

  menu_init();
  variables_init();

  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);  

  #if ENABLED(DISPLAY)
    display_init();
  #endif

  #if ENABLED(SDCARD)
    Serial.print("Initializing SD card...");
  
    if (!SD.begin(SD_CS)) {
      Serial.println("failed!");
      return;
    }
    Serial.println("SD OK!");
  
    #if ENABLED(SPLASH_SCREEN_ENABLED)
      bmpDraw(SPLASH_IMAGE, 0, 0);
      delay(SPLASH_DURATION);
      tft.fillScreen(BLACK);
    #endif

    sd_new_logfile();
  
  #endif

  #if ENABLED(DISPLAY)

    if(nfc_init() && gps_init() && eeprom_init()) {
      tft.setTextColor(GREEN);
      tft.println("All systems pass");   
    } else {
      tft.setTextColor(RED);
      tft.setTextSize(3);
      tft.println("Error");   
      tft.setTextSize(1);
      while(1 < 2);
    }
    tft.setTextColor(WHITE); 

    delay(500);

    tft.fillScreen(BLACK);  
    
  #endif

  randomSeed(analogRead(0));
}

void loop(void) {

  loop_timing(); 

  if(loop_index == 0) {
    calculate_voltage();
    loop_index++;
  } else if (loop_index == 1) {
    gps_update();
    loop_index++;
  } else if (loop_index == 2) {
    analog_update();
    loop_index++;
  } else if (loop_index == 3) {
    log_to_sd();
    loop_index = 0;
  }

  button_update();
  menu_buttons();   


  if(menu_active) {
    menu_draw();
  } else {
    status_draw();
  }
    
  //nfc_update();
  update_actions();

  randNumber = random(10);

  if(randNumber == 2) {
    displayVariables[V_SPEED].floatVal += (random(-500,500)/1000.0);
    displayVariables[V_SPEED].valChanged = true;
  }
  

}

void sd_new_logfile() {
  for (int n = 0; n < LIMIT; n++) {
    sprintf(fileName, "TLOG%.3d.TXT", n);
    if (SD.exists(fileName)) continue;
    dataFile = SD.open(fileName, O_CREAT | O_WRITE);    
    break;
  }
}

void status_draw() {

  bool blankScreen = false;

  if (button_up.pressedFor(LONG_PRESS) && indicatorsJustChanged == false) {
    Serial.println("Changing big variable...");
    activeVariableBig++;
    indicatorsJustChanged = true;
    blankScreen = true;
  
    if (activeVariableBig >= NUM_VARIABLES)
      activeVariableBig = 0;
  }

  if(menu_changed) {
    blankScreen = true;
    menu_changed = false;
    indicatorsJustChanged = true;
  }


  if(blankScreen) {
    tft.fillScreen(BLACK);  
    print_variable_big(activeVariableBig, 20, 20, 2, WHITE);
  } 
  
  if(indicatorsJustChanged) {
    displayVariables[activeVariableBig].valChanged = true;
  } 
    
  print_variable_big(activeVariableBig, 20, 20, 2, WHITE);

  if(indicatorsJustChanged && button_up.wasReleased())
    indicatorsJustChanged = false;
  

 
}

void calculate_voltage() {
  int Vin = (analogRead(BATT_PIN) * 5)/1024;
  int prevVolts = displayVariables[V_VOLTS].intVal;
  displayVariables[V_VOLTS].intVal = (Vin * (BATT_R1 + BATT_R2)) / BATT_R2;

  if(abs(displayVariables[V_VOLTS].intVal - prevVolts) != 0)
    displayVariables[V_VOLTS].valChanged = true;
}

// Performs actions requested by user via menu
void update_actions() {
    if(menuArray[INVERT_DISPLAY].curState == 1)
      tft.invert(true);
    else
      tft.invert(false);

    //clear EEPROM if instructed via menu. Wait until menu has updated to show status symbol.
    if(menuArray[CLEAR_EEPROM].curState == 1 && menu_changed == false) {
      clear_eeprom();
      menuArray[CLEAR_EEPROM].curState = 0;
      menu_changed = true;
      menu_changed_small = true;
  }

  if(menuArray[NEW_LOGFILE].curState == 1 && menu_changed == false) {
      if (dataFile) {
        dataFile.close();  
      }

      sd_new_logfile();
      
      menuArray[NEW_LOGFILE].curState = 0;
      menu_changed = true;
      menu_changed_small = true;
  }


}

void menu_draw() {
  if(menu_changed) {

    //if only part of the menu has changed, only redraw that part
    if(menu_changed_small) {
      if(menu_selected_index > 0)
        tft.fillRect(0,(menu_selected_index-1)*MENU_OPTION_HEIGHT,128,MENU_OPTION_HEIGHT*2,BLACK);
      else {
        tft.fillRect(0,menu_selected_index*MENU_OPTION_HEIGHT,128,MENU_OPTION_HEIGHT,BLACK);
        tft.fillRect(0,(MENU_OPTIONS-1)*MENU_OPTION_HEIGHT,128,MENU_OPTION_HEIGHT,BLACK);
      }
    } else {
      tft.fillScreen(BLACK);
    }
    for(int i = 0; i < MENU_OPTIONS; i++) {
      int temp_y = i*MENU_OPTION_HEIGHT;
      if(menu_selected_index == i) {
        tft.fillRect(0, temp_y , 128, MENU_OPTION_HEIGHT, WHITE);
        tft.setTextColor(BLACK);
        tft.fillTriangle(5, i*MENU_OPTION_HEIGHT+2, 5, (i+1)*MENU_OPTION_HEIGHT-2, 12, (i+0.5)*MENU_OPTION_HEIGHT, RED);
      }
      else {
        tft.setTextColor(WHITE);
      }
      tft.setCursor(15,i*MENU_OPTION_HEIGHT);
      tft.print(menuArray[i].optionName);
      tft.setCursor(100,i*MENU_OPTION_HEIGHT);

      tft.print(menuArray[i].stateText[menuArray[i].curState]);
  
    }

    menu_changed = false;
    menu_changed_small = false;
  }
}

void menu_buttons() {

  if (button_set.pressedFor(LONG_PRESS) && menu_just_opened == false) {
    menu_active = !menu_active;
    
    menu_changed = true;
    menu_changed_small = false;

    Serial.print("Menu: ");
    Serial.println(menu_active);

    menu_just_opened = true;
  }

  if(menu_just_opened && button_set.wasReleased())
    menu_just_opened = false;


  if(button_set.wasPressed()) {
    menu_selected_index++;
    if (menu_selected_index >= MENU_OPTIONS) {
      menu_selected_index = 0;
    }
    menu_changed = true;
    menu_changed_small = true;
  }

  if (button_sel.wasPressed()) {
    menuArray[menu_selected_index].curState++;
  
    if(menuArray[menu_selected_index].curState >= menuArray[menu_selected_index].numStates)
      menuArray[menu_selected_index].curState = 0;
      
    menu_changed = true;
    menu_changed_small = true;
  
    #if ENABLED(EEPROM_ENABLED)
      EEPROM.update(eeprom_options_start_addr + menu_selected_index, menuArray[menu_selected_index].curState);
    #endif
  }
}

void displayPrint(char *text, uint16_t color) {
  if(tft.getCursorY()>tft.height()-10) {
    tft.fillScreen(BLACK);
    tft.setCursor(0,0);
  }
  tft.setTextColor(color);
  tft.print(text);
  Serial.print(text);
}

void displayTest() {
  for(int i = 0; i < 100; i++) {
    displayPrint("Test ",WHITE);
    tft.println(i);
    
    delay(500);
  }
}

void beginTestPrint(char *moduleName) {
  tft.print(moduleName);
  tft.print("...");
  for(int i = 0; i < 14-strlen(moduleName); i++)
    tft.print(" ");
}

void completeTestPrint(bool pass) {
  if(!pass) {
    tft.setTextColor(RED);
    tft.print("FAIL");
  } else {
    tft.setTextColor(GREEN);
    tft.print("pass");
  }
  tft.setTextColor(WHITE);
}

bool display_init() {
  tft.begin();
  //tft.setRotation(1);
  tft.fillScreen(BLACK);
  //tft.setTextColor(WHITE);
  //tft.print("Display...    ");
  //tft.setTextColor(GREEN);
  //tft.println("pass"); 
  tft.setTextColor(WHITE);
  return true;
}

bool nfc_init() {
  #if ENABLED(NFC)
    beginTestPrint("NFC");
    nfc.begin();
     
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (! versiondata) {
      completeTestPrint(false); 
      return false;
    }
    
    // Got ok data, print it out!
    completeTestPrint(true); 
    
    #ifdef VERBOSE_DISPLAY
    
    tft.print("Found chip PN5"); tft.println((versiondata>>24) & 0xFF, HEX); 
    tft.print("Firmware ver. "); tft.print((versiondata>>16) & 0xFF, DEC); 
    tft.print('.'); tft.println((versiondata>>8) & 0xFF, DEC);
    
    #endif
    
    // Set the max number of retry attempts to read from a card
    // This prevents us from waiting forever for a card, which is
    // the default behaviour of the PN532.
    nfc.setPassiveActivationRetries(0xFF);
    
    // configure board to read RFID tags
    nfc.SAMConfig();
  #endif
  
  return true;

}

void nfc_update() {
  #if ENABLED(NFC)
    
    boolean success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
    // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
    // 'uid' will be populated with the UID, and uidLength will indicate
    // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
    
    if (success) {
      tft.println("Found a card!");
      tft.print("UID Length: ");
      
      tft.print(uidLength, DEC);
      tft.print(" bytes");
  
      tft.print("UID Value: ");
      for (uint8_t i=0; i < uidLength; i++) 
      {
        tft.print(" 0x");
        tft.print(uid[i], HEX);
      }
      tft.println("");
      // Wait 1 second before continuing
      delay(1000);
    }
    else
    {
      // PN532 probably timed out waiting for a card
      //displayPrintln("Timed out waiting for a card", WHITE);
    }

  #endif
  
}

bool gps_init() {
  #if ENABLED(GPS)
    beginTestPrint("GPS");
    Serial1.begin(GPSBaud);
    
    unsigned long gps_starttime = millis();
    
    while(millis() - gps_starttime < 2000) {
      while (Serial1.available() > 0)
        gps.encode(Serial1.read());
    }
    
    if(gps.charsProcessed() < 10) {
        completeTestPrint(false);
      return false;
    } else {
        completeTestPrint(true);
      return true;
    }
  #else
    return true;
  #endif
}

void gps_update() {
  #if ENABLED(GPS)
    while (Serial1.available() > 0)
      gps.encode(Serial1.read());
  #endif
}

void gps_print() {
  #if ENABLED(GPS)
    //int16_t oldCursorX = tft.getCursorX;
    //int16_t oldCursorY = tft.getCursorY;
    
    int bar_y = 110;
    
    tft.fillRect(0, bar_y , tft.width(), tft.height()-bar_y, BLACK);
    
    tft.setCursor(0,bar_y);
    
    tft.print(gps.time.hour());
    tft.print(":");
    tft.print(gps.time.minute());
    tft.print(":");
    tft.print(gps.time.second());
    
    //tft.setCursor(oldCursorX,oldCursorY);
  #endif

}


void button_update() {
  button_up.read();
  button_sel.read();
  button_set.read();
}

void loop_timing()
{
  while(micros() - last_loop_micros < LOOP_DELAY - 10)
  {
    delayMicroseconds(20); // This loop timing is inaccurate and does not take processing time into account. A better timing function is required.
  }
  //unsigned long temp_last_micros = last_loop_micros;
  last_loop_micros = micros();
  //Serial.println(last_loop_micros-temp_last_micros);
}

bool eeprom_init() {
  #if ENABLED(EEPROM_ENABLED)
    beginTestPrint("EEPROM");

    int eeAddress = eeprom_options_start_addr;
    int tempVar;
    Serial.println("Initialising EEPROM...");

    for(int i = 0; i < MENU_OPTIONS; i++) {
      EEPROM.get(eeAddress + i, tempVar);

      Serial.print(menuArray[i].optionName);
      Serial.print(": ");

      if (tempVar >= 0 && tempVar < menuArray[i].numStates) {
        menuArray[i].curState = tempVar;
        Serial.println(menuArray[i].stateText[tempVar]);  
      } else {
        Serial.println("Error - invalid value!");
      }


      delay(200);
    }
  
    completeTestPrint(true);
  #endif
  
  return true;
  
}

void menu_init() {  
  menuArray[GPS_ONOFF].optionName = "GPS Tracking";
  menuArray[GPS_ONOFF].numStates = 2;
  menuArray[GPS_ONOFF].curState = 0;
  menuArray[GPS_ONOFF].stateText[0] = "Off";
  menuArray[GPS_ONOFF].stateText[1] = "On";

  menuArray[LED_BRIGHTNESS].optionName = "LED Brightness";
  menuArray[LED_BRIGHTNESS].numStates = 4;
  menuArray[LED_BRIGHTNESS].curState = 0;
  menuArray[LED_BRIGHTNESS].stateText[0] = "Off";
  menuArray[LED_BRIGHTNESS].stateText[1] = "Low";
  menuArray[LED_BRIGHTNESS].stateText[2] = "Med";
  menuArray[LED_BRIGHTNESS].stateText[3] = "Hi";

  menuArray[SD_LOGGING].optionName = "Log to SD";
  menuArray[SD_LOGGING].numStates = 2;
  menuArray[SD_LOGGING].curState = 0;
  menuArray[SD_LOGGING].stateText[0] = "Off";
  menuArray[SD_LOGGING].stateText[1] = "On";

  menuArray[INVERT_DISPLAY].optionName = "Invert Display";
  menuArray[INVERT_DISPLAY].numStates = 2;
  menuArray[INVERT_DISPLAY].curState = 0;
  menuArray[INVERT_DISPLAY].stateText[0] = "Off";
  menuArray[INVERT_DISPLAY].stateText[1] = "On";

  menuArray[CLEAR_EEPROM].optionName = "Clear EEPROM";
  menuArray[CLEAR_EEPROM].numStates = 2;
  menuArray[CLEAR_EEPROM].curState = 0;
  menuArray[CLEAR_EEPROM].stateText[0] = "";
  menuArray[CLEAR_EEPROM].stateText[1] = "...";

  menuArray[NEW_LOGFILE].optionName = "New Logfile";
  menuArray[NEW_LOGFILE].numStates = 2;
  menuArray[NEW_LOGFILE].curState = 0;
  menuArray[NEW_LOGFILE].stateText[0] = "";
  menuArray[NEW_LOGFILE].stateText[1] = "...";

}

void variables_init() {
  displayVariables[V_SPEED].type = FLOAT;
  displayVariables[V_SPEED].floatVal = 75;
  displayVariables[V_SPEED].displaySuffix = "km/h";
  displayVariables[V_SPEED].displaySymbol = "VEL";
  displayVariables[V_SPEED].decPoints = 1;
  displayVariables[V_SPEED].valChanged = true;
  displayVariables[V_SPEED].symOffset = 0;

  displayVariables[V_SPEED_AVG].type = FLOAT;
  displayVariables[V_SPEED_AVG].floatVal = 5;
  displayVariables[V_SPEED_AVG].displaySuffix = "km/h";
  displayVariables[V_SPEED_AVG].displaySymbol = "AVG";
  displayVariables[V_SPEED_AVG].decPoints = 1;
  displayVariables[V_SPEED_AVG].valChanged = true;
  displayVariables[V_SPEED_AVG].symOffset = 20;

  displayVariables[V_RPM].type = INT;
  displayVariables[V_RPM].intVal = 5000;
  displayVariables[V_RPM].displaySymbol = "RPM";
  displayVariables[V_RPM].valChanged = true;
  displayVariables[V_RPM].symOffset = 40;

  displayVariables[V_VOLTS].type = INT;
  displayVariables[V_VOLTS].intVal = 0;
  displayVariables[V_VOLTS].displaySymbol = "BAT";
  displayVariables[V_VOLTS].displaySuffix = "V";
  displayVariables[V_VOLTS].valChanged = true;
  displayVariables[V_VOLTS].symOffset = 60;
}

String string_variable(int var) {
  if(displayVariables[var].type == INT)
    return String(displayVariables[var].intVal);
  if(displayVariables[var].type == FLOAT)
    return String(displayVariables[var].floatVal,displayVariables[var].decPoints);
  if(displayVariables[var].type == BOOL)
    return String(displayVariables[var].boolVal);
  else
    return displayVariables[var].stringVal;
}

String formatted_variable(int var) {
  String formatted;
  
  formatted = displayVariables[var].displayPrefix;
  
  formatted += string_variable(var);

  formatted += displayVariables[var].displaySuffix;

  return formatted;
}

void print_variable_big(int var, int x, int y, int t_size, uint16_t color) {

  if(displayVariables[var].valChanged == true) {
    
    String variableValue = formatted_variable(var);
  
    int symOffsetX = 0+displayVariables[var].symOffset;
    int symOffsetY = -10;
    int width = (variableValue.length() * (5 * t_size)) - symOffsetX;
    int height = (7 * t_size) - symOffsetY;
  
    tft.fillRect(x+symOffsetX,y+symOffsetY,width,height,BLACK);
  
    tft.setCursor(x,y);
    tft.setTextColor(color);
    tft.setTextSize(t_size);
  
    tft.print(variableValue);
    
    tft.setTextSize(1);
    tft.setCursor(x+symOffsetX,y+symOffsetY);
    
    tft.print(displayVariables[var].displaySymbol);

    displayVariables[var].valChanged = false;
  }
  
}

void clear_eeprom() {
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  Serial.println("EEPROM Cleared!");
}


// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: "); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: "); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        for (row=0; row<h; row++) { // For each scanline...
          tft.goTo(x, y+row);

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          // optimize by setting pins now
          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];

            tft.drawPixel(x+col, y+row, tft.Color565(r,g,b));
            // optimized!
            //tft.pushColor(tft.Color565(r,g,b));
          } // end pixel
        } // end scanline
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println("BMP format not recognized.");
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void analog_update() {
  analog1 = analogRead(ANALOG1);
  analog2 = analogRead(ANALOG2);
  analog3 = analogRead(ANALOG3);
  analog4 = analogRead(ANALOG4);
  analog5 = analogRead(ANALOG5);
  analog6 = analogRead(ANALOG6);
}

void log_to_sd() {
  if(menuArray[SD_LOGGING].curState == 1) {  
    
    String dataString = "";
  
    //log system time
    dataString += String(millis()); 
    dataString += "\t";

    for(int i = 0; i < NUM_VARIABLES; i++) {
      dataString += string_variable(i);
      dataString += "\t";
    }
  
    //log analog values
    dataString += String(analog1);
    dataString += "\t";
    dataString += String(analog2);
    dataString += "\t";
    dataString += String(analog3);
    dataString += "\t";
    dataString += String(analog4);
    dataString += "\t";
    dataString += String(analog5);
    dataString += "\t";
    dataString += String(analog6);
    dataString += "\t";
  
    //log GPS data
    dataString += String(gps.date.isValid());
    dataString += "\t";
  
    char sz[32] = "**********";
  
    if(gps.date.isValid()) {
      sprintf(sz, "%02d/%02d/%02d ", gps.date.month(), gps.date.day(), gps.date.year());
    }
    
    
    dataString += String(sz);
    dataString += "\t";
  
    char st[32] = "********";
      
    if(gps.time.isValid()) {
      sprintf(sz, "%02d:%02d:%02d ", gps.time.hour(), gps.time.minute(), gps.time.second());
    }
    
    dataString += String(sz);
    dataString += "\t";
    dataString += String(gps.location.isValid());
    dataString += "\t";
    dataString += String(gps.location.lat());
    dataString += "\t";
    dataString += String(gps.location.lng());
    dataString += "\t";
  
    dataFile = SD.open(fileName, O_CREAT | O_APPEND | O_WRITE);
    
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();  
      // print to the serial port too:
      //Serial.println(dataString);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening file for logging");
      sd_new_logfile();
    }
  }
}


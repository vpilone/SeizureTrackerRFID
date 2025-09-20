//Adafruit PN532 Library
#include <Adafruit_PN532.h>

// Include the RTC library
#include "RTC.h"

//Include the NTP library
#include <NTPClient.h>

#if defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>
#elif defined(ARDUINO_UNOWIFIR4)
#include <WiFiS3.h>
#endif

#include <WiFiUdp.h>
#include "arduino_secrets.h"

//defining pins for I2C Shield connection (as per example)
#define PN532_IRQ (2)
#define PN532_RESET (3)

#define LEDR_PIN (13)
#define LEDG_PIN (12)
#define LEDB_PIN (11)

#define NR_SHORTSECTOR (32)          // Number of short sectors on Mifare 1K/4K
#define NR_LONGSECTOR (8)            // Number of long sectors on Mifare 4K
#define NR_BLOCK_OF_SHORTSECTOR (4)  // Number of blocks in a short sector
#define NR_BLOCK_OF_LONGSECTOR (16)  // Number of blocks in a long sector

// Determine the sector trailer block based on sector number
#define BLOCK_NUMBER_OF_SECTOR_TRAILER(sector) (((sector) < NR_SHORTSECTOR) ? ((sector)*NR_BLOCK_OF_SHORTSECTOR + NR_BLOCK_OF_SHORTSECTOR - 1) : (NR_SHORTSECTOR * NR_BLOCK_OF_SHORTSECTOR + (sector - NR_SHORTSECTOR) * NR_BLOCK_OF_LONGSECTOR + NR_BLOCK_OF_LONGSECTOR - 1))

// Determine the sector's first block based on the sector number
#define BLOCK_NUMBER_OF_SECTOR_1ST_BLOCK(sector) (((sector) < NR_SHORTSECTOR) ? ((sector)*NR_BLOCK_OF_SHORTSECTOR) : (NR_SHORTSECTOR * NR_BLOCK_OF_SHORTSECTOR + (sector - NR_SHORTSECTOR) * NR_BLOCK_OF_LONGSECTOR))

// The default Mifare Classic key
static const uint8_t KEY_DEFAULT_KEYAB[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

int wifiStatus = WL_IDLE_STATUS;
WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp);

//define Adafruit instance for IC2 connection
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//url variables
const char urlSystem[] = "uidev.seizuretracker.com";
const uint8_t urlSystemSize = sizeof(urlSystem);
const char variableSystem[] = "RFID?ID=";
const uint8_t variableSystemSize = sizeof(variableSystem);
//url size size of above plus 15 random charecters and 1 "/" charecters, minus the null charecter at the end of the first variable
const uint8_t urlSize = urlSystemSize + variableSystemSize + 16;

//formatting variables (designed for mifare ultralight EV1 128b)
uint8_t formattingData[32] = { 0xE1, 0x10, 0x10, 0x00 };

void setup(void) {
  //Begin Serial (has to be on 115200 to ensure read write functions)
  pinMode(LEDR_PIN, OUTPUT);
  pinMode(LEDG_PIN, OUTPUT);
  pinMode(LEDB_PIN, OUTPUT);
  digitalWrite(LEDB_PIN, HIGH);
  Serial.begin(115200);
  while (!Serial)
    ;

  RTC.begin();
  uint8_t success = connectToWiFi();
  if (success) {
    Serial.println("\nStarting connection to server...");
    timeClient.begin();
    timeClient.update();

    // Get the current date and time from an NTP server and convert
    // it to UTC +2 by passing the time zone offset in hours.
    // You may change the time zone offset to your local one.
    auto timeZoneOffsetHours = 0;
    auto unixTime = timeClient.getEpochTime() + (timeZoneOffsetHours * 3600);
    Serial.print("Unix time = ");
    Serial.println(unixTime);
    RTCTime timeToSet = RTCTime(unixTime);
    RTC.setTime(timeToSet);
  }

  // Retrieve the date and time from the RTC and print them
  RTCTime currentTime;
  RTC.getTime(currentTime);

  //Intial Output for Debugging
  Serial.println("\n\nHello!");
  Serial.println("The RTC is currently: " + String(currentTime));

  digitalWrite(LEDB_PIN, LOW);
  digitalWrite(LEDG_PIN, HIGH);

  //begin nfc connection
  nfc.begin();
}

void loop() {
  //variable definition
  uint8_t userPages = 0;                    //variable to track user pages for error checks
  uint8_t success;                          //boolean to store if a card is found
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Waits for a compatible card, then stores its uid
  Serial.println("Waiting for an ISO14443A Card ...");
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    analogWrite(LEDG_PIN, LOW);
    digitalWrite(LEDR_PIN, LOW);
    //prints the UID (card tracking)
    Serial.print("  UID Length: ");
    Serial.print(uidLength, DEC);
    Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    Serial.println("Tap the same card again to overwrite it.");

    //variables for double tap feature
    uint8_t tempuid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t tempuidLength;
    //max time in seconds *4
    uint8_t maxSeconds = 10 * 4;

    //wait until no card is found (card has been removed) or timeout
    while (success && maxSeconds > 0) {
      analogWrite(LEDG_PIN, !digitalRead(LEDR_PIN) * 100);
      digitalWrite(LEDR_PIN, !digitalRead(LEDR_PIN));
      delay(250);
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempuid, &tempuidLength, 250);
      maxSeconds--;
    }

    //wait until card is refound or timeout
    while (!success && maxSeconds > 0) {
      analogWrite(LEDG_PIN, !digitalRead(LEDR_PIN) * 100);
      digitalWrite(LEDR_PIN, !digitalRead(LEDR_PIN));
      delay(250);
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempuid, &tempuidLength, 50);
      maxSeconds--;
    }

    //check if the attempt timed out
    if (maxSeconds < 1) {
      Serial.println("Timeout, please tap the card.");
      analogWrite(LEDG_PIN, 255);
      return;
    } else if (!compareUID(uid, uidLength, tempuid, tempuidLength)) {
      Serial.println("Please tap the same card to overwrite it.");
      //throw error
      ledError();
      return;
    } else {
      digitalWrite(LEDR_PIN, HIGH);
      analogWrite(LEDG_PIN, 100);
    }

    if (uidLength == 7) {
      //loop for each avalible page, starts on page 4 to avoid protected area
      //creates buffer for data
      uint8_t data[32];

      //checks if the card has been formatted
      success = nfc.mifareultralight_ReadPage(3, data);
      if (success) {
        if (data[0] == 0xE1 && data[1] == 0x10) {
          Serial.println("Card has been formatted.");
        } else if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
          Serial.println("Card will be formatted.");
          nfc.mifareultralight_WritePage(3, formattingData);
        } else {
          Serial.println("Card has been formatted incorrectly.");
          ledError();
          return;
        }
      }

      for (uint8_t i = 4; i < 35; i++) {
        // //attempts to erase page
        memset(data, 0, 4);
        success = nfc.mifareultralight_WritePage(i, data);

        // // // Display the results, depending on 'success'
        if (success) {
          userPages++;
        }
        //catch lost connection error
        else {
          Serial.println("Unable to read the requested page!");
          //currrently, want to abort this loop as soon as the connection is lost
          break;
        }
      }
      Serial.flush();

      //rewrites the device, using https://www. as default protocol (aka 0x02)
      char url[urlSize];
      assembleURL(url);
      Serial.println(url);
      //uint8_t written = nfc.mifareultralight_WritePage(uint8_t page, uint8_t *data)
      uint8_t written = nfc.ntag2xx_WriteNDEFURI(0x04, url, userPages * 4);
      if (written == 1) {
        Serial.println("Successfully wrote to device.");
      } else {
        Serial.println("Error writing to card.");
        ledError();
        return;
      }
      Serial.flush();
    }

    //different methods for mifaire classic cards
    //IMPORTANT: This code has to reformat all cards to write to them
    else if (uidLength == 4) {
      //loop for each avalible page, starts on page 4 to avoid protected area
      //creates buffer for data
      uint8_t data[16];

      //reformat based on example code
      success = mifaireclassic_ndeftoclassic();

      //catch error with reformatting
      if (!success) {
        Serial.println("Something went wrong.");
        ledError();
        return;
      }

      //for classics, authenticate first
      Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 0, 0, keya);
      if (success) {
        Serial.flush();

        //write to card
        //uses https://www. as default protocol (aka 0x02)
        char url[urlSize];
        assembleURL(url);
        Serial.println(url);
        success = nfc.mifareclassic_FormatNDEF();
        if (!success) {
          Serial.println("Failed to reformat");
          ledError();
          return;
        }
        success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
        if (!success) {
          Serial.println("Authentication failed.");
          ledError();
          return;
        }
        uint8_t written = nfc.mifareclassic_WriteNDEFURI(1, 0x04, url);
        if (written == 1) {
          Serial.println("Successfully wrote to device.");
        } else {
          Serial.println("Error writing to card.");
          ledError();
          return;
        }
        Serial.flush();
      }
    }
  }

  //prep to scan another card
  digitalWrite(LEDR_PIN, LOW);
  Serial.flush();
  for (uint8_t i = 0; i < 3 * 2; i++) {
    analogWrite(LEDG_PIN, 255);
    delay(250);
    analogWrite(LEDG_PIN, 0);
    delay(250);
  }
  analogWrite(LEDG_PIN, 255);
}

//method to assemble the url
void assembleURL(char* urlArray) {
  uint8_t currentPos = 0;

  //adds the system first
  while (currentPos < urlSystemSize - 1) {
    urlArray[currentPos] = urlSystem[currentPos];
    currentPos++;
  }

  urlArray[currentPos] = '/';
  currentPos++;

  //adds the variable system
  while (currentPos < urlSize - 17) {
    urlArray[currentPos] = variableSystem[currentPos - urlSystemSize];
    currentPos++;
  }

  urlArray[currentPos] = 'R';
  currentPos++;

  urlArray[currentPos] = 'F';
  currentPos++;

  //adds a series of 6 random numbers and 6 date numbers
  while (currentPos < urlSize - 9) {
    uint8_t randAlphaNum = random(1, 63);
    if (randAlphaNum < 11) {
      randAlphaNum += 47;
    } else if (randAlphaNum > 10 && randAlphaNum < 37) {
      randAlphaNum += 54;
    } else {
      randAlphaNum += 60;
    }
    urlArray[currentPos] = char(randAlphaNum);
    currentPos++;
  }

  //get current date and add it to the end
  RTCTime currentTime;
  RTC.getTime(currentTime);
  uint8_t curYear = currentTime.getYear() % 100;

  urlArray[currentPos] = char(curYear/10 + 48);
  currentPos++;

  urlArray[currentPos] = (char)(curYear%10 + 48);
  currentPos++;

  uint8_t curMonth = Month2int(currentTime.getMonth());

  urlArray[currentPos] = char(curMonth/10 + 48);
  currentPos++;

  urlArray[currentPos] = (char)(curMonth%10 + 48);
  currentPos++;

  uint8_t curDay = currentTime.getDayOfMonth();

  urlArray[currentPos] = char(curDay/10 + 48);
  currentPos++;

  urlArray[currentPos] = (char)(curDay%10 + 48);
  currentPos++;

  urlArray[currentPos] = 'I';
  currentPos++;

  urlArray[currentPos] = 'D';
  currentPos++;

  urlArray[currentPos] = 0x00;
}

//method to compare two UID
bool compareUID(uint8_t* uid1, uint8_t uid1Length, uint8_t* uid2, uint8_t uid2Length) {
  if (uid1Length != uid2Length) {
    return false;
  }

  for (uint8_t i = 0; i < uid1Length; i++) {
    if (uid1[i] != uid2[i]) {
      return false;
    }
  }
  return true;
}

//method to format mifaireclassic cards
//0 means error, 1 means success
uint8_t mifaireclassic_ndeftoclassic() {
  uint8_t success;                          // Flag to check if there was an error with the PN532
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t blockBuffer[16];                  // Buffer to store block contents
  uint8_t blankAccessBits[3] = { 0xff, 0x07, 0x80 };
  uint8_t idx = 0;
  uint8_t numOfSector = 16;  // Assume Mifare Classic 1K for now (16 4-block sectors)

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  for (idx = 0; idx < numOfSector; idx++) {
    // Step 1: Authenticate the current sector using key B 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
    success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, BLOCK_NUMBER_OF_SECTOR_TRAILER(idx), 1, (uint8_t*)KEY_DEFAULT_KEYAB);
    if (!success) {
      Serial.print("Authentication failed for sector ");
      Serial.println(numOfSector);
      return 0;
    }

    // Step 2: Write to the other blocks
    if (idx == 16) {
      memset(blockBuffer, 0, sizeof(blockBuffer));
      if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)) - 3, blockBuffer))) {
        Serial.print("Unable to write to sector ");
        Serial.println(numOfSector);
        return 0;
      }
    }
    if ((idx == 0) || (idx == 16)) {
      memset(blockBuffer, 0, sizeof(blockBuffer));
      if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)) - 2, blockBuffer))) {
        Serial.print("Unable to write to sector ");
        Serial.println(numOfSector);
        return 0;
      }
    } else {
      memset(blockBuffer, 0, sizeof(blockBuffer));
      if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)) - 3, blockBuffer))) {
        Serial.print("Unable to write to sector ");
        Serial.println(numOfSector);
        return 0;
      }
      if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)) - 2, blockBuffer))) {
        Serial.print("Unable to write to sector ");
        Serial.println(numOfSector);
        return 0;
      }
    }
    memset(blockBuffer, 0, sizeof(blockBuffer));
    if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)) - 1, blockBuffer))) {
      Serial.print("Unable to write to sector ");
      Serial.println(numOfSector);
      return 0;
    }

    // Step 3: Reset both keys to 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
    memcpy(blockBuffer, KEY_DEFAULT_KEYAB, sizeof(KEY_DEFAULT_KEYAB));
    memcpy(blockBuffer + 6, blankAccessBits, sizeof(blankAccessBits));
    blockBuffer[9] = 0x69;
    memcpy(blockBuffer + 10, KEY_DEFAULT_KEYAB, sizeof(KEY_DEFAULT_KEYAB));

    // Step 4: Write the trailer block
    if (!(nfc.mifareclassic_WriteDataBlock((BLOCK_NUMBER_OF_SECTOR_TRAILER(idx)), blockBuffer))) {
      Serial.print("Unable to write trailer block of sector ");
      Serial.println(numOfSector);
      return 0;
    }
  }
  Serial.println("\n\nDone!");
  delay(1000);
  Serial.flush();
  return 1;
}

void ledError() {
  analogWrite(LEDG_PIN, 0);
  delay(250);
  for (uint8_t i = 0; i < 15 * 4; i++) {
    digitalWrite(LEDR_PIN, !digitalRead(LEDR_PIN));
    delay(250);
  }
  digitalWrite(LEDR_PIN, 0);
  analogWrite(LEDG_PIN, 255);
}

void wifiError() {
  digitalWrite(LEDB_PIN, HIGH);
  for (uint8_t i = 0; i < 15 * 4; i++) {
    digitalWrite(LEDB_PIN, !digitalRead(LEDB_PIN));
    delay(250);
  }
  digitalWrite(LEDB_PIN, HIGH);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

uint8_t connectToWiFi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    wifiError();
    return 0;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  wifiStatus = WiFi.begin(ssid, pass);

  // wait 10 seconds for connection:
  delay(10000);

  if (wifiStatus != WL_CONNECTED) {
    Serial.println("Could not connect to wifi.");
    wifiError();
    return 0;
  } else {

    Serial.println("Connected to WiFi");
    printWifiStatus();
  }
  return 1;
}

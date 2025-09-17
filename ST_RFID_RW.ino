//Adafruit PN532 Library
#include <Adafruit_PN532.h>

//defining pins for I2C Shield connection (as per example)
#define PN532_IRQ (2)
#define PN532_RESET (3)

#define LED_PIN (13)

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

//define Adafruit instance for IC2 connection
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//url variables
const char urlSystem[] = "uidev.seizuretracker.com";
const uint8_t urlSystemSize = sizeof(urlSystem);
const char variableSystem[] = "RFID?ID=";
const uint8_t variableSystemSize = sizeof(variableSystem);
//url size size of above plus 15 random charecters and 1 "/" charecters, minus the null charecter at the end of the first variable
const uint8_t urlSize = urlSystemSize + variableSystemSize + 15;

void setup(void) {
  //Begin Serial (has to be on 115200 to ensure read write functions)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);

  //Intial Output for Debugging
  Serial.println("\n\nHello!");

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
    digitalWrite(LED_PIN, LOW);
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
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(250);
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempuid, &tempuidLength, 250);
      maxSeconds--;
    }

    //wait until card is refound or timeout
    while (!success && maxSeconds > 0) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(250);
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempuid, &tempuidLength, 50);
      maxSeconds--;
    }

    //check if the attempt timed out
    if (maxSeconds < 1) {
      Serial.println("Timeout, please tap the card.");
      digitalWrite(LED_PIN, HIGH);
      return;
    } else if (!compareUID(uid, uidLength, tempuid, tempuidLength)) {
      Serial.println("Please tap the same card to overwrite it.");
      digitalWrite(LED_PIN, HIGH);
      return;
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    if (uidLength == 7) {
      //loop for each avalible page, starts on page 4 to avoid protected area
      //creates buffer for data
      uint8_t data[32];
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
        }
        success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
        if (!success) {
          Serial.println("Authentication failed.");
        }
        char url2[] = "uidev.seizuretracker.com/RFID?ID=33203";
        uint8_t written = nfc.mifareclassic_WriteNDEFURI(1, 0x04, url2);
        if (written == 1) {
          Serial.println("Successfully wrote to device.");
        } else {
          Serial.println("Error writing to card.");
        }
        Serial.flush();
      }
    }
  }

  //prep to scan another card
  Serial.flush();
  for (uint8_t i = 0; i < 3 * 4; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(250);
  }
  digitalWrite(LED_PIN, HIGH);
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
  while (currentPos < urlSize - 16) {
    urlArray[currentPos] = variableSystem[currentPos - urlSystemSize];
    currentPos++;
  }

  //adds a series of 15 random numbers
  while (currentPos < urlSize - 1) {
    urlArray[currentPos] = char(random(48, 58));
    currentPos++;
  }

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

//mifare classic write URI (modified to support longer URI)
// uint8_t mifareclassic_WriteNDEFURI(uint8_t sectorNumber,
//                                                    uint8_t uriIdentifier,
//                                                    const char* url) {
//   // Figure out how long the string is
//   uint8_t len = strlen(url);

//   // Make sure we're within a 1K limit for the sector number
//   if ((sectorNumber < 1) || (sectorNumber > 15))
//     return 0;

//   // Make sure the URI payload is between 1 and 54 chars
//   if ((len < 1) || (len > 54))
//     return 0;

//   // Note 0xD3 0xF7 0xD3 0xF7 0xD3 0xF7 must be used for key A
//   // in NDEF records

//   // Setup the sector buffer (w/pre-formatted TLV wrapper and NDEF message)
//   uint8_t sectorbuffer1[16] = { 0x00,
//                                 0x00,
//                                 0x03,
//                                 (uint8_t)(len + 5),
//                                 0xD1,
//                                 0x01,
//                                 (uint8_t)(len + 1),
//                                 0x55,
//                                 uriIdentifier,
//                                 0x00,
//                                 0x00,
//                                 0x00,
//                                 0x00,
//                                 0x00,
//                                 0x00,
//                                 0x00 };
//   uint8_t sectorbuffer2[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//   uint8_t sectorbuffer3[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//   uint8_t sectorbuffer4[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//   uint8_t sectorbuffer5[16] = { 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07,
//                                 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
//   if (len <= 6) {
//     // Unlikely we'll get a url this short, but why not ...
//     memcpy(sectorbuffer1 + 9, url, len);
//     sectorbuffer1[len + 9] = 0xFE;
//   } else if (len == 7) {
//     // 0xFE needs to be wrapped around to next block
//     memcpy(sectorbuffer1 + 9, url, len);
//     sectorbuffer2[0] = 0xFE;
//   } else if ((len > 7) && (len <= 22)) {
//     // Url fits in two blocks
//     memcpy(sectorbuffer1 + 9, url, 7);
//     memcpy(sectorbuffer2, url + 7, len - 7);
//     sectorbuffer2[len - 7] = 0xFE;
//   } else if ((len > 22) && (len <= 37)) {
//     // url fits in three block
//     memcpy(sectorbuffer1 + 9, url, 7);
//     memcpy(sectorbuffer2, url + 7, len - 7);
//     sectorbuffer3[0] = 0xFE;
//   } else if (len == 38) {
//     // 0xFE needs to be wrapped around to final block
//     memcpy(sectorbuffer1 + 9, url, 7);
//     memcpy(sectorbuffer2, url + 7, 16);
//     memcpy(sectorbuffer3, url + 23, len - 24);
//     sectorbuffer4[len-24] = 0xFE;
//   } else {
//     // Url fits in four blocks
//     memcpy(sectorbuffer1 + 9, url, 7);
//     memcpy(sectorbuffer2, url + 7, 16);
//     memcpy(sectorbuffer3, url + 23, 16);
//     memcpy(sectorbuffer4, url + 38, len - 39);
//     sectorbuffer4[len - 37] = 0xFE;
//   }

//   // Now write all four blocks back to the card
//   if (!(nfc.mifareclassic_WriteDataBlock(sectorNumber * 4, sectorbuffer1)))
//     return 0;
//   if (!(nfc.mifareclassic_WriteDataBlock((sectorNumber * 4) + 1, sectorbuffer2)))
//     return 0;
//   if (!(nfc.mifareclassic_WriteDataBlock((sectorNumber * 4) + 2, sectorbuffer3)))
//     return 0;
//   if (!(nfc.mifareclassic_WriteDataBlock((sectorNumber * 4) + 3, sectorbuffer4)))
//     return 0;

//   // Seems that everything was OK (?!)
//   return 1;
// }

// uint8_t mifareultralight_WriteNDEFURI() {
  
//}


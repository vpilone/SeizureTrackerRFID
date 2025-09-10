//Adafruit PN532 Library
#include <Adafruit_PN532.h>

//defining pins for I2C Shield connection (as per example)
#define PN532_IRQ   (2)
#define PN532_RESET (3)

//define Adafruit instance for IC2 connection
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//future functions to know
//nfc.ntag2xx_WriteNDEFURI - used to write URI to Band, must already be formatted, 0x02 should be https://www.

void setup(void) {
  //Begin Serial (has to be on 115200 to ensure read write functions)
  Serial.begin(115200);

  //Intial Output for Debugging
  Serial.println("\n\nHello!");

  nfc.begin();

  Serial.println("Waiting for an ISO14443A Card ...");
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    //write to card
    char url[] = "vinnypilone.com";
    bool temp = nfc.ntag2xx_WriteNDEFURI(0x02, url, 39);
    Serial.println(temp);
  }
}

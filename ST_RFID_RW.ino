//Adafruit PN532 Library
#include <Adafruit_PN532.h>

//defining pins for I2C Shield connection (as per example)
#define PN532_IRQ   (2)
#define PN532_RESET (3)

//define Adafruit instance for IC2 connection
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void setup(void) {
  //Begin Serial (has to be on 115200 to ensure read write functions)
  Serial.begin(115200);

  //Intial Output for Debugging
  Serial.println("\n\nHello!");

  //begin nfc connection
  nfc.begin();
}

void loop() {
  //variable definition
  uint8_t userPages = 0; //variable to track user pages for error checks
  uint8_t success; //boolean to store if a card is found
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Waits for a compatible card, then stores its uid
  Serial.println("Waiting for an ISO14443A Card ...");
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    //prints the UID (card tracking)
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    //boolean to find old URI/URL
    bool uriFound = false; 

    //creates buffer for data
    uint8_t data[32];

    //loop for each avalible page, starts on page 4 to avoid protected area
    for (uint8_t i = 3; i < 42; i++)
      {
        //attempts to read page
        success = nfc.ntag2xx_ReadPage(i, data);

        // Display the results, depending on 'success'
        if (success)
        {
          userPages++;
          //performs various actions based on state
          for (uint8_t i = 0; i < 4; i++) {
            //if the ending hex 0xFE is found, set uriFound to false;
            if (data[i] == 0xFE) {
              Serial.println(" ");
              uriFound = false;
            }
            //if a uri has been found, print the next charecter
            if (uriFound == true) {
              Serial.print((char)data[i]);
            }
            //if a uri hasn't been found, check if the next charecter is the start of a uri
            if (data[i] == 0x55 && uriFound == false) {
              i = i + 1; //skips connection protocol hex
              Serial.print("Old URI: ");
              uriFound = true;
            }
          }
        }
        //catch lost connection error
        else
        {
          Serial.println("Unable to read the requested page!");
          uriFound = false;
        }
      }
    }

    //ask the user if they want to overwrite the card
    Serial.println("Would you like to overwrite this card? (y/n)");
    Serial.flush();
    Serial.end();
    Serial.begin(115200);
    uint8_t response[8];
    while (!Serial.available());
    while (Serial.available()) {
    response[0] = Serial.read();
    }

    //if y or Y, overwrite the card and display the success or failure
    if (response[0] == 0x59 || response[0] == 0x79) {
      //write to card
      char url[] = "seizuretracker.com";
      //uses https://www. as default protocol (aka 0x02)
      uint8_t written = nfc.ntag2xx_WriteNDEFURI(0x02, url, userPages);
      if (written == 1) {
        Serial.println("Successfully wrote to device.");
      }
      else {
        Serial.println("Error writing to card.");
      }
    }
    Serial.flush();

    //ask to scan another card
    Serial.println("\n\nSend a character to scan another tag!");
    Serial.flush();
    Serial.end();
    Serial.begin(115200);
    while (!Serial.available());
    while (Serial.available()) {
    Serial.read();
    }
    Serial.flush();
  }

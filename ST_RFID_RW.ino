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
          //currrently, want to abort this loop as soon as the connection is lost
          break;
        }
      }
    }

    //ask the user if they want to overwrite the card
    Serial.println("This card will be overwritten in 5 seconds, send a char to stop it.");
    //variable to store response
    //variable to track wait times
    uint8_t response[8] = {0,0,0,0,0,0,0,0};
    uint8_t delaySeconds = 5;

    Serial.flush();
    Serial.end();
    Serial.begin(115200);
    //waits for a given amount of time before overwriting the device
    while (!Serial.available() && delaySeconds > 0) {
      delay(1000);
      delaySeconds--;
    }
    while (Serial.available()) {
    response[0] = Serial.read();
    }

    //if not overriden, rewrites the device
    if (response[0] == 0) {
      //url variables
      const char system[] = "seizuretracker.com";
      const uint8_t systemSize = sizeof(system);
      const char resourceLocation[] = "directory";
      const uint8_t resourceLocationSize = sizeof(resourceLocation);
      //url size size of above plus 5 random numbers and 2 "/" charecters, minus the null charecter at the end of the first variable
      const uint8_t urlSize = systemSize + resourceLocationSize + 6;

      //assemble url based on variables
      char url[urlSize];
      uint8_t currentPos = 0;

      //adds the system first
      while (currentPos < systemSize - 1) {
        url[currentPos] = system[currentPos];
        currentPos++;
      }
      url[currentPos] = '/';
      currentPos++;

      //adds a series of 5 random numbers
      while (currentPos < systemSize + 5) {
        url[currentPos] = char(random(48,58));
        currentPos++;
      }
      url[currentPos] = '/';
      currentPos++;

      //adds the directory
      while (currentPos < urlSize - 1) {
        url[currentPos] = resourceLocation[currentPos - systemSize - 6];
        currentPos++;
      }
      
      //write to card
      //uses https://www. as default protocol (aka 0x02)
      Serial.println(url);
      uint8_t written = nfc.ntag2xx_WriteNDEFURI(0x02, url, userPages*4);
      if (written == 1) {
        Serial.println("Successfully wrote to device.");
      }
      else {
        Serial.println("Error writing to card.");
      }
    }
    else {
      Serial.println("Didn't write to card.");
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

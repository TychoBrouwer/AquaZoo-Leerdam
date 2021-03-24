/*
    AquaZoo Light Control w/ Arduino

    This program was made with an Arduino Mega 2560 in mind.
    It uses a W5100 chip arduino ethernet shield to open a webserver.
    For the timestamp it uses a DS3231 RTC.
    On the webserver you are able to update I/O pins on the Arduino.
    The last values of the I/O pins gets stored in EEPROM for saving after shutdown.
    The program also logs a timestamp with any actions as a csv file to the SD-card on the ethernet shield.
    And it displays certain values and errors on an lcd screen.

    Author: Tycho Brouwer
    School: Fortes Lyceum Gorinchem
    Client: AquaZoo Leerdam
    Library's:
        Regexp (Nick Gammon)
        Neotimer (Jose Rullan)
        ds3231 (Petre Rodan)
*/

// Library for compatibility with the SPI protocol (which the Ethernet shield uses)
#include <SPI.h>
// Library for the W5100 based Ethernet shield
#include <Ethernet.h>
// Library for regular expressions (for URI matching)
#include <Regexp.h>
// Library for the SD card slot on the Ethernet shield
#include <SD.h>
// Library for compatibility with the I2C protocol (which the RTC uses)
#include <Wire.h>
// Library for the DS3231 RTC
#include <ds3231.h>
// Library for the lcd screen
#include <LiquidCrystal.h>
// Library for non-blocking timer
#include <neotimer.h>
// Library for EEPROM communication
#include <EEPROM.h>

// define the select pin for the ethernet shield's ethernet (10 for W5100 chip)
#define ethernetPin (byte)10
// define the select pin for the SD card on the ethernet shield (4 for W5100 chip)
#define SDPin 4
// define the regular expression for the lamps URI
#define regexLamps "l(%d+)=([a-z]+)&"
// define the regular expression for the time URI
#define regexTime "data=date(%d+)-(%d+)-(%d+)time(%d+)-(%d+)-(%d+)"
// define the password for the webserver, the password can be changed here without any other configuration
#define password "AquaZooLeerdam"
// define the number of the first I/O that has a lamp connected (further lamps should be on following I/O pins)
#define lampPinOffset (byte)22
// define the number of lamps
#define lampNumber (byte)7
// define eeprom starting address
#define startAddressLamps (byte)10
// define the max character lenght of the webserver URI
#define URIMaxLengh (80 + (7 * lampNumber) + sizeof(password))
// define lcd pin variables
#define rsLcd (byte)9
#define enLcd (byte)8
#define d4Lcd (byte)7
#define d5Lcd (byte)6
#define d6Lcd (byte)5
#define d7Lcd (byte)3

// webserver mac adress
byte mac[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
// define server port (default for webservers: 80)
EthernetServer server(80);
// define variable to store current status lamps
String lamps[lampNumber];
// define variable to store which lamps are to be update
boolean updatedLamps[lampNumber];
// define variable to store the RTC time object
struct ts t;
// define variable to store the SD card File object
File logFile;
// define variable to store the lcd object
LiquidCrystal lcd(rsLcd, enLcd, d4Lcd, d5Lcd, d6Lcd, d7Lcd);
// define lcd message variables
char message[17] = "Initialized";
char serverAddress[15];
bool error = false;
// define timer variables
Neotimer initTimer = Neotimer(3000);
// define eeprom lamps (true for turned on)
bool eepromLamps[lampNumber];

void setup()
{
    // Open serial communications
    Serial.begin(115200);

    // open wire communications for I2C-protocol
    Wire.begin();

    // run lcd screen setup
    lcdSetup();
    // run RTC setup
    RTCSetup();
    // run SD card setup
    SDSetup();
    // run ethernet setup
    ethernetSetup();

    logToSD(F("AquaZoo Light Control w/ Arduino"), true, true, false);
    logToSD(F("Initialized"), true, true, false);

    // set pinout for LEDs
    for (byte i = lampPinOffset; i < (lampNumber + lampPinOffset); i++) {
        pinMode(i, OUTPUT);
    }

    // create lamp status array
    for (byte i = 0; i < lampNumber; i++) {
        lamps[i] = F("off");
        updatedLamps[i] = false;
    }

    logToSD(F(""), false, true, false);

    // start timer for intialized text
    initTimer.start();
}

void loop()
{
    // run the html and webserver logic
    serveHTML();

    // print message to lcd
    setLcd(message);

    // check for finished timer intialized text (dont if error to keep error on display)
    if (initTimer.done() && !error) {
        initTimer.stop();

        strcpy(message, serverAddress);
    }
}

void updateLamps()
{
    for (byte i = 0; i < lampNumber; i++) {
        // turn off the not updated lamps
        if (updatedLamps[i] == false) {
            lamps[i] = "off";
        }

        updatedLamps[i] = false;

        // update I/O pins
        byte pin = i + lampPinOffset;

        // set the I/O pin HIGH or LOW
        if (lamps[i] == "on") {
            digitalWrite(pin, HIGH);

            // add lamp number index to eeprom index
            eepromLamps[i] = true;
        } else {
            digitalWrite(pin, LOW);

            eepromLamps[i] = false;
        }

        logToSD(F("lamp "), true, false, false);
        logToSD(String(i), false, false, false);
        logToSD(F(": "), false, false, false);
        logToSD(lamps[i], false, true, false);
    }

    logToSD(F(""), false, true, false);
}

/*
    AquaZoo Light Control w/ Arduino

    This program was made with an Arduino Mega 2560 in mind.
    It uses a W5100 chip arduino ethernet shield to open a webserver.
    For the timestamp it uses a DS3231 RTC
    On the webserver you are able to update I/O pins on the Arduino.
    The last values of the I/O pins gets stored in EEPROM for saving after shutdown
    The program also logs a timestamp with any actions as a csv file to the SD-card on the ethernet shield.
    And it displays certain values and errors on an lcd screen

    Author: Tycho Brouwer
    School: Fortes Lyceum Gorinchem
    Client: AquaZoo Leerdam
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
// define the password for the webserver
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
        }
        else {
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

void ethernetSetup()
{
    Ethernet.init(ethernetPin);

    // start the Ethernet connection and the server
    Ethernet.begin(mac);

    // Check for Ethernet hardware present (show error on lcd if so)
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        logToSD(F("Ethernet shield was not found.  Sorry, can't run without hardware."), true, true, false);

        strcpy(message, "Eth init failed");

        error = true;
    }

    // check for ethernet cable connected (show error on lcd if so)
    if (Ethernet.linkStatus() == LinkOFF) {
        logToSD(F("Ethernet cable is not connected."), true, true, false);

        strcpy(message, "Eth disconnected");

        error = true;
    }

    // start the server
    server.begin();

    // save severIpAdress
    sprintf(serverAddress, "%d.%d.%d.%d", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
}

void printHTML(EthernetClient client, boolean loginPage)
{
    DS3231_get(&t);
    char date[10], time[8];

    // format the TRC variables to a string
    sprintf(date, "%d-%.2d-%.2d", t.year, t.mon, t.mday);
    sprintf(time, "%.2d:%.2d:%.2d", t.hour, t.min, t.sec);

    // send a standard http response header
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println();

    // begin html document
    client.print(F("<!DOCTYPE HTML><html><meta name='viewport' content='width=device-width,initial-scale=1'><style>*{font-family:Arial,Helvetica,sans-serif}form{margin:10vh 10vw;display:flex;flex-wrap:wrap;}label input{position:absolute;opacity:0;cursor:pointer}label{height:40px;position:relative;margin:0 20px;cursor:pointer}span{position:absolute;width:30px;height:30px;background:#eee;position:absolute;border:1px #777 solid;text-align:center;line-height:30px}span:hover{background:#ccc;transition:.3s}label input:checked~span{background-color:#2196f3}</style>"));
    client.print(F("<div>"));
    client.print(F("<form action='/' method='get'>"));

    // if logged in
    if (loginPage == false) {
        // print for each lamp and add checked if lamp is on
        for (byte i = 0; i < lampNumber; i++) {
            client.print(F("<label>"));

            if (lamps[i] == "on") {
                client.print(F("<input type='checkbox' checked name='l"));
            }
            else {
                client.print(F("<input type='checkbox' name='l"));
            }

            client.print(i);
            client.print(F("' id='"));
            client.print(i);
            client.print(F("'><span>"));
            client.print(i + 1);
            client.print(F("</span></label>"));
        }

        client.print(F("<input type='submit' name='submit' value='update lamps' style='margin-left:30px;'>"));
        client.print(F("<input id='dataInput' style='display:none;' type='text' name='data' value=''>"));
        client.print(F("</form>"));
        // print script for updating the time inside HTML input to get it after request is receaved
        client.print(F("<script type='text/javascript'>setInterval(() => { const d=new Date(); document.getElementById('dataInput').value = `date${d.getFullYear()}-${(d.getMonth() + 1)}-${d.getDate()}time${d.getHours()}-${d.getMinutes()}-${d.getSeconds()}pws"));
        client.print(password);
        client.print(F("`; }, 500);</script>"));
    }
    else {
        client.print(F("<input type='text' name='psw'>"));
        client.print(F("<input type='submit' name='submit' value='login'>"));
        client.print(F("</form>"));
    }

    client.print(F("</div>"));
    client.print(F("</html>"));
}

void serveHTML()
{
    // listen for incoming clients
    EthernetClient client = server.available();

    if (client) {
        logToSD(F("new webserver request"), true, true, true);
        logToSD(F(""), false, true, false);

        // readString to save received header information
        String readString;
        readString.reserve(URIMaxLengh);

        boolean currentLineIsBlank = true, urlReceived = false, loginPage = true;

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();

                // add new received character to String
                if (urlReceived == false) {
                    readString += c;
                }

                // check if whole URI is received and right password
                // urlReceived == false to only run once
                // remove the favicon request
                if (urlReceived == false && readString.indexOf(password) > 0 && readString.indexOf(F("favicon")) < 0) {
                    logToSD(readString, true, true, false);

                    urlReceived = true;
                    loginPage = false;

                    uint8_t count;

                    // convert readString to a char for the regular expression
                    char buf[URIMaxLengh];
                    readString.toCharArray(buf, URIMaxLengh);

                    MatchState ms(buf);

                    // if lamps where updated
                    if (readString.indexOf("update+lamps") > 0) {

                        // regular expression and regexp_callback function to process the request
                        count = ms.GlobalMatch(regexLamps, regexp_callback);

                        // update the lamp pins
                        updateLamps();
                    }

                    // if date/time update is in URI
                    if (readString.indexOf("data=date") > 0) {
                        // regular expression and regexp_callback function to process the request
                        count = ms.GlobalMatch(regexTime, regexp_callback);

                        // update the time
                        RTCSetTime();
                    }

                    readString = "";
                }

                // if all headers received print HTML to client
                if (c == '\n' && currentLineIsBlank) {
                    printHTML(client, loginPage);

                    break;
                }

                if (c == '\n') {
                    currentLineIsBlank = true;
                }
                else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            }
        }

        // close the connection
        delay(1);
        client.stop();
    }
}

void lcdSetup()
{
    // initiate lcd screen
    lcd.begin(16, 2);
    lcd.clear();
}

void setLcd(char* string)
{
    // get time from RTC
    DS3231_get(&t);
    char time[8];

    // format the TRC variables to a string
    sprintf(time, "        %.2d:%.2d:%.2d", t.hour, t.min, t.sec);

    // print time to lcd
    lcd.setCursor(0, 0);
    lcd.print(time);

    // print message to lcd
    lcd.setCursor(0, 1);
    lcd.print(string);
}

void regexp_callback(const boolean match, const boolean length, const MatchState& ms)
{
    byte index;
    String value;

    uint8_t intValueYear;
    byte intValueMon;

    boolean timeUpdate = false;

    for (byte i = 0; i < ms.level; i++) {
        char cap[12];

        // get the i-ste capure
        ms.GetCapture(cap, i);

        // sepperate index and value for lamps
        if (i == 0) {
            index = atoi(cap);
            intValueYear = atoi(cap);
        }
        else if (i == 1) {
            value = cap;
            intValueMon = atoi(cap);
        }
        else if (i == 2) {
            timeUpdate = true;
            t.mday = atoi(cap);
        }
        else if (i == 3) {
            t.hour = atoi(cap);
        }
        else if (i == 4) {
            t.min = atoi(cap);
        }
        else if (i == 5) {
            t.sec = atoi(cap);
        }
    }

    // check which was updated time or lamps
    if (timeUpdate == false) {
        lamps[index] = value;
        updatedLamps[index] = true;
    }
    else {
        t.year = intValueYear;
        t.mon = intValueMon;
    }
}

void RTCSetup()
{
    // initiate ds3231 library
    DS3231_init(DS3231_CONTROL_INTCN);

    // to sync TRC module time
    // t.hour = 22;
    // t.min = 50;
    // t.sec = 0;
    // t.mday = 26;
    // t.mon = 1;
    // t.year = 2021;
    //
    // DS3231_set(t);
}

void RTCSetTime()
{
    // update RTC time
    DS3231_set(t);
}

void SDSetup()
{
    if (!SD.begin(SDPin)) {
        Serial.println(F("SD card module failed to initiate"));

        strcpy(message, "SD init failed");

        error = true;
    }
}

void logToSD(String value, bool first, bool last, bool comma)
{
    DS3231_get(&t);

    char fileName[25];

    // make filename with date for logs
    sprintf(fileName, "%d%.2d%.2d.csv", t.year, t.mon, t.mday);

    // open file
    logFile = SD.open(fileName, FILE_WRITE);

    if (logFile) {
        if (first) {
            // get TRC time
            DS3231_get(&t);
            char date[10];
            char time[8];

            // format the TRC variables to a string
            sprintf(date, "%d/%d/%d", t.mday, t.mon, t.year);
            sprintf(time, "%d:%d:%d", t.hour, t.min, t.sec);

            logFile.print(date);
            logFile.print(",");
            logFile.print(time);
            logFile.print(",");
            Serial.print(date);
            Serial.print(",");
            Serial.print(time);
            Serial.print(",");
        }

        if (last) {
            logFile.println(value);
            Serial.println(value);
        }
        else {
            logFile.print(value);
            Serial.print(value);

            if (comma) {
                logFile.print(",");
                Serial.print(",");
            }
        }

        logFile.close();
    }
    else {
        // if the file didn't open, print an error
        Serial.println(F("error opening logFile"));

        strcpy(message, "SD init failed");

        error = true;
    }
}
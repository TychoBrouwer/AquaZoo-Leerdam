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
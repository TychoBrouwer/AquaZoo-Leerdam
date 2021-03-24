void lcdSetup()
{
    // initiate lcd screen
    lcd.begin(16, 2);
    lcd.clear();
}

void setLcd(char *string)
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

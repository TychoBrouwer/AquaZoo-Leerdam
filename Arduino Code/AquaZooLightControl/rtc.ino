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

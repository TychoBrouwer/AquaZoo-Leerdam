void regexp_callback(const boolean match, const boolean length, const MatchState & ms)
{
    byte index;
    String value;

    uint8_t intValueYear;
    byte intValueMon;

    boolean timeUpdate = false;

    for (byte i = 0; i < ms.level; i++) {
        char cap[12];

        // get the i-ste capure
        ms.GetCapture (cap, i);

        // sepperate index and value for lamps
        if (i == 0) {
            index = atoi(cap);
            intValueYear = atoi(cap);
        } else if (i == 1) {
            value = cap;
            intValueMon = atoi(cap);
        } else if (i == 2) {
            timeUpdate = true;
            t.mday = atoi(cap);
        } else if (i == 3) {
            t.hour = atoi(cap);
        } else if (i == 4) {
            t.min = atoi(cap);
        } else if (i == 5) {
            t.sec = atoi(cap);
        }
    }

    // check which was updated time or lamps
    if (timeUpdate == false) {
        lamps[index] = value;
        updatedLamps[index] = true;
    } else {
        t.year = intValueYear;
        t.mon = intValueMon;
    }
}

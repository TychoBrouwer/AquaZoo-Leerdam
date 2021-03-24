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
            } else {
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
    } else {
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

                    MatchState ms (buf);

                    // if lamps where updated
                    if (readString.indexOf("update+lamps") > 0) {

                        // regular expression and regexp_callback function to process the request
                        count = ms.GlobalMatch (regexLamps, regexp_callback);

                        // update the lamp pins
                        updateLamps();
                    }

                    // if date/time update is in URI
                    if (readString.indexOf("data=date") > 0) {
                        // regular expression and regexp_callback function to process the request
                        count = ms.GlobalMatch (regexTime, regexp_callback);

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
                } else if (c != '\r') {
                currentLineIsBlank = false;
                }
            }
        }

        // close the connection
        delay(1);
        client.stop();
    }
}

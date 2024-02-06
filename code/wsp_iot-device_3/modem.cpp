/**
 * @file      modem.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-21
 *
 */

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#include "settings.h"
#include "./certs/EMQX_root_CA.h"

#if defined(USING_MODEM)

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(Serial1);
#endif

int8_t ret;

bool getLoaction();
void loactionTask(void *);
bool isConnect();
void getPsmTimer();
void writeCaFiles(int index, const char *filename, const char *data, size_t lenght);
void testModem();
void setupNBIoTNetwork();
void networkRegistration();
void checkNetworkBearer();
void showModemInfo();
void writeCerts();
void setup_TLS_SSL();
void mqttConnect();
void publishMsg();

void setupModem(){
    Serial.println("Initializing modem...");
    
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);

    /*digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);*/

    testModem();
    Serial.println("Modem started!");

    if (modem.getSimStatus() != SIM_READY){
        Serial.println("SIM Card is not insert!!!");
        return;
    }else{
        Serial.println("SIM Card is insert !");
    }

    setupNBIoTNetwork();

    networkRegistration();

    checkNetworkBearer();

    /*if (modem.enableGPS() == false) {
        Serial.println("Enable gps failed!");
    }*/

    //xTaskCreate(loactionTask, "gps", 4096, NULL, 10, NULL);
}

void loactionTask(void *){
    Serial.println("Get location");
    while (1) {
        if (getLoaction()) {
            Serial.println();
            break;
        }
        Serial.print(".");
        delay(2000);
    }
    vTaskDelete(NULL);
}

bool getLoaction(){
    float lat      = 0;
    float lon      = 0;
    float speed    = 0;
    float alt      = 0;
    int   vsat     = 0;
    int   usat     = 0;
    float accuracy = 0;
    int   year     = 0;
    int   month    = 0;
    int   day      = 0;
    int   hour     = 0;
    int   min      = 0;
    int   sec      = 0;

    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy,
                     &year, &month, &day, &hour, &min, &sec)) {
        Serial.println();
        Serial.print("lat:"); Serial.print(String(lat, 8)); Serial.print("\t");
        Serial.print("lon:"); Serial.print(String(lon, 8)); Serial.println();
        Serial.print("speed:"); Serial.print(speed); Serial.print("\t");
        Serial.print("alt:"); Serial.print(alt); Serial.println();
        Serial.print("year:"); Serial.print(year);
        Serial.print(" month:"); Serial.print(month);
        Serial.print(" day:"); Serial.print(day);
        Serial.print(" hour:"); Serial.print(hour);
        Serial.print(" min:"); Serial.print(min);
        Serial.print(" sec:"); Serial.print(sec); Serial.println();
        Serial.println();
        return true;
    }
    return false;
}

bool isConnect(){
    modem.sendAT("+SMSTATE?");
    if (modem.waitResponse("+SMSTATE: ")) {
        String res = modem.stream.readStringUntil('\r');
        return res.toInt();
    }
    return false;
}

// retrieve the Power Saving Mode (PSM) timer value from the modem
void getPsmTimer(){
    modem.sendAT("+CPSMS?"); // Send AT command to query PSM settings

    if (modem.waitResponse("+CPSMS:") != 1)
    {
        Serial.println("Failed to retrieve PSM timer");
        return;
    }

    String response = modem.stream.readStringUntil('\r');
    // Parse the response to extract PSM timer values (T3412, T3324)

    Serial.print("PSM Timer values: ");
    Serial.println(response);
}

/* Define the writeCaFiles function in this C++ code snippet takes four parameters: an integer index, a const char pointer filename, a const char pointer data, and a size_t variable length.
The function is designed to write the provided data to a file with a specified name. */
void writeCaFiles(int index, const char *filename, const char *data, size_t lenght){
    modem.sendAT("+CFSTERM");
    modem.waitResponse();


    modem.sendAT("+CFSINIT");
    if (modem.waitResponse() != 1) {
        Serial.println("INITFS FAILED");
        return;
    }
    // AT+CFSWFILE=<index>,<filename>,<mode>,<filesize>,<input time>
    // <index>
    //      Directory of AP filesystem:
    //      0 "/custapp/" 1 "/fota/" 2 "/datatx/" 3 "/customer/"
    // <mode>
    //      0 If the file already existed, write the data at the beginning of the
    //      file. 1 If the file already existed, add the data at the end o
    // <file size>
    //      File size should be less than 10240 bytes. <input time> Millisecond,
    //      should send file during this period or you can’t send file when
    //      timeout. The value should be less
    // <input time> Millisecond, should send file during this period or you can’t
    // send file when timeout. The value should be less than 10000 ms.

    size_t payloadLenght = lenght;
    size_t totalSize     = payloadLenght;
    size_t alardyWrite   = 0;

    while (totalSize > 0) {
        size_t writeSize = totalSize > 10000 ? 10000 : totalSize;

        modem.sendAT("+CFSWFILE=", index, ",", "\"", filename, "\"", ",",
                     !(totalSize == payloadLenght), ",", writeSize, ",", 10000);
        modem.waitResponse(30000UL, "DOWNLOAD");
REWRITE:
        modem.stream.write(data + alardyWrite, writeSize);
        if (modem.waitResponse(30000UL) == 1) {
            alardyWrite += writeSize;
            totalSize -= writeSize;
            Serial.printf("Writing:%d overage:%d\n", writeSize, totalSize);
        } else {
            Serial.println("Write failed!");
            delay(1000);
            goto REWRITE;
        }
    }

    Serial.println("Wirte done!!!");

    modem.sendAT("+CFSTERM");
    if (modem.waitResponse() != 1) {
        Serial.println("CFSTERM FAILED");
        return;
    }
}

void testModem(){
    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 5) {
            Serial.println("Warn : try reinit modem!");
            // Pull down PWRKEY for more than 1 second according to manual requirements
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            // modem.sendAT("+CRESET");
            Serial.println("Power cycling modem");
            retry = 0;
        }
    }
}

void setupNBIoTNetwork(){
    Serial.println("Start to set the network mode to NB-IOT ");
    modem.setNetworkMode(2);  // use automatic
    modem.setPreferredMode(MODEM_NB_IOT);
    uint8_t pre = modem.getPreferredMode();
    uint8_t mode = modem.getNetworkMode();
    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);
}

void networkRegistration(){
    Serial.println("Configuring APN...");
    modem.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
    modem.waitResponse();

    RegStatus s;
    do {
        s = modem.getRegistrationStatus();
        int16_t sq = modem.getSignalQuality();

        if ( s == REG_SEARCHING) {
            Serial.print("Searching...");
        } else {
            Serial.print("Other code:");
            Serial.print(s);
            break;
        }
        Serial.print("  Signal:");
        Serial.println(sq);
        delay(1000);
    } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

    Serial.println();
    Serial.print("Network register info:");
    if (s >= sizeof(register_info) / sizeof(*register_info)) {
        Serial.print("Other result = ");
        Serial.println(s);
    } else {
        Serial.println(register_info[s]);
    }
}

void checkNetworkBearer(){
    // Check the status of the network bearer
    Serial.println("Checking the status of network bearer ...");
    modem.sendAT("+CNACT?"); // Send the AT command to query the network bearer status
    String response;
    ret = modem.waitResponse(10000UL, response); // Wait for the response with a 10-second timeout

    bool alreadyActivated = false;
    if (response.indexOf("+CNACT: 0,1") >= 0) // Check if the response contains "+CNACT: 0,1" indicating bearer is activated
    {
        Serial.println("Network bearer is already activated");
        alreadyActivated = true;
    }
    else if (response.indexOf("+CNACT: 0,0") >= 0) // Check if the response contains "+CNACT: 0,0" indicating bearer is deactivated
    {
        Serial.println("Network bearer is not activated");
    }

    if (!alreadyActivated)
    {
        // Activating network bearer
        Serial.println("Activating network bearer ...");
        modem.sendAT("+CNACT=0,1"); // Send the AT command to activate the network bearer
        response = "";
        ret = modem.waitResponse(10000UL, response); // Wait for the response with a 10-second timeout

        if (response.indexOf("ERROR") >= 0) // Check if the response contains "ERROR"
        {
            Serial.println("Network bearer activation failed");
        }
        else if (response.indexOf("OK") >= 0) // Check if the response contains "OK"
        {
            Serial.println("Activation in progress, waiting for network response...");

            // Wait for the "+APP PDP: 0,ACTIVE" response
            bool activationConfirmed = false;
            unsigned long startTime = millis();
            while (millis() - startTime < 60000UL) // Wait for 60 seconds
            {
                if (modem.stream.available())
                {
                    response = modem.stream.readString();
                    if (response.indexOf("+APP PDP: 0,ACTIVE") >= 0)
                    {
                        activationConfirmed = true;
                        break;
                    }
                }
                delay(100);
            }
            if (activationConfirmed)
            {
                Serial.println("Network bearer is activated successfully !");
            }
            else
            {
                Serial.println("No network response within the timeout");
            }
        }
        else
        {
            Serial.println("No valid response");
        }
    }
    // Ping the Google DNS server
    modem.sendAT("+SNPING4=\"8.8.8.8\",1,16,5000");
    if (modem.waitResponse(10000L) != 1){
        Serial.println("Ping Failed!");
        return;
    }else{
        Serial.println(response);
    }
}

void showModemInfo(){
    Serial.println("T-SIM7080G Firmware Version: ");
    modem.sendAT("+CGMR");
    String response;
    if (modem.waitResponse(10000L, response) != 1){
        Serial.println("Get Firmware Version Failed!");
    }else{
        Serial.println(response);
    }

    String ccid = modem.getSimCCID();
    Serial.print("CCID:");
    Serial.println(ccid);

    String imei = modem.getIMEI();
    Serial.print("IMEI:");
    Serial.println(imei);

    String imsi = modem.getIMSI();
    Serial.print("IMSI:");
    Serial.println(imsi);

    String cop = modem.getOperator();
    Serial.print("Operator:");
    Serial.println(cop);

    IPAddress local = modem.localIP();
    Serial.print("Local IP:");
    Serial.println(local);

    int csq = modem.getSignalQuality();
    Serial.print("Signal quality:");
    Serial.println(csq);

    modem.sendAT("+CGNAPN");
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("Get APN Failed!");
        return;
    }

    modem.sendAT("+CCLK?");
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("Get time Failed!");
        return;
    }
}

void writeCerts(){
    writeCaFiles(3, "rootCA.pem", root_CA, strlen(root_CA));                 // root_CA is retrieved from Mosquitto_root_CA.h, which is downloaded from https://test.mosquitto.org/ssl/mosquitto.org.crt
    //writeCaFiles(3, "deviceCert.crt", Client_CRT, strlen(Client_CRT));       // Client_CRT is retrieved from Mosquitto_Client_CRT.h, please follow the guide to generate the device certificate in https://test.mosquitto.org/ssl/
    //writeCaFiles(3, "devicePrivateKey.pem", Client_PSK, strlen(Client_PSK)); // Client_PSK is retrieved from Mosquitto_Client_PSK.h, please follow the guide to generate the device certificate and private key in https://test.mosquitto.org/ssl/
}

void setup_TLS_SSL(){
    // If it is already connected, disconnect it first
    modem.sendAT("+SMDISC");
    if (modem.waitResponse() != 1) {}

    modem.sendAT("+SMCONF=\"URL\",", server, ",", port);
    if (modem.waitResponse() != 1) { return; }

    modem.sendAT("+SMCONF=\"KEEPTIME\",60");
    if (modem.waitResponse() != 1) {}

    modem.sendAT("+SMCONF=\"CLEANSS\",1");
    if (modem.waitResponse() != 1) {}

    modem.sendAT("+SMCONF=\"CLIENTID\",", clientID);
    if (modem.waitResponse() != 1) { return; }

    modem.sendAT("+SMCONF=\"USERNAME\",\"", username, "\"");
    if (modem.waitResponse() != 1) { return; }

    modem.sendAT("+SMCONF=\"PASSWORD\",\"", password, "\"");
    if (modem.waitResponse() != 1) { return; }

    // AT+CSSLCFG="SSLVERSION",<ctxindex>,<sslversion>
    modem.sendAT("+CSSLCFG=\"SSLVERSION\",0,3");
    if (modem.waitResponse() != 1) { return; }

    modem.sendAT("+CSSLCFG=\"SNI\",0,", server);
    if (modem.waitResponse() != 1) { return; }

    // <ssltype>
    //      1 QAPI_NET_SSL_CERTIFICATE_E
    //      2 QAPI_NET_SSL_CA_LIST_E
    //      3 QAPI_NET_SSL_PSK_TABLE_E
    // AT+CSSLCFG="CONVERT",2,"rootCA.pem"
    modem.sendAT("+CSSLCFG=\"CONVERT\",2,\"rootCA.pem\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert rootCA.pem failed!");
        //return;
    }

    // AT+SMSTATE?
    // modem.sendAT("+CSSLCFG=\"CONVERT\",1,\"server.crt\",\"key.pem\"");
    // if (modem.waitResponse() != 1) {
    //     Serial.println("Convert server.crt failed!");
    // }

    /*
    Defined Values
    <index> SSL status, range: 0-6
            0 Not support SSL
            1-6 Corresponding to AT+CSSLCFG command parameter <ctindex>
            range 0-5
    <ca list> CA_LIST file name, Max length is 20 bytes
    <cert name> CERT_NAME file name, Max length is 20 bytes
    <len_calist> Integer type. Maximum length of parameter <ca list>.
    <len_certname> Integer type. Maximum length of parameter <cert name>.
    */
    modem.sendAT("+SMSSL=1,\"rootCA.pem\",\"\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert ca failed!");
    }
}

void mqttConnect(){
    Serial.println("Connecting to MQTT server ...");
    while (true){
        // Before connecting, you need to confirm that the time has been synchronized.
        //modem.sendAT("+CCLK?");
        //modem.waitResponse(30000);

        modem.sendAT("+SMCONN");
        String response;
        modem.waitResponse(10000UL, response);
        Serial.println(response);

        if (response.indexOf("ERROR") >= 0) // Check if the response contains "ERROR"
        {
            Serial.println("Connect failed");
            break; // Stop attempting to connect
        }
        else if (response.indexOf("OK") >= 0) // Check if the response contains "OK"
        {
            Serial.println("Connect successfully");
            break; // Exit the loop
        }
        else
        {
            Serial.println("No valid response, retrying connect ...");
            delay(1000);
        }
    }
}

void publishMsg(String status){
    testModem();

    // Enable RF
    modem.sendAT("+CFUN=1");
    if (modem.waitResponse(20000UL) != 1) {
        Serial.println("Enable RF Failed!");
    }else{
        Serial.println("RF enabled");
    }

    setupNBIoTNetwork();
    networkRegistration();
    checkNetworkBearer();
    setup_TLS_SSL();
    mqttConnect();

    // Publish fake data
    String payload = "";
    const int randMax = 35;
    const int randMin = 18;
    char buffer[1024] = {0};

    payload.concat(clientID);

    payload.concat(",");
    payload.concat(modem.getGSMDateTime(DATE_FULL));

    payload.concat(",SQ,");
    int csq = modem.getSignalQuality();
    Serial.print("Signal quality:");
    Serial.println(csq);
    payload.concat(csq);

    payload.concat(status);

    payload.concat(",CPSI,");
    modem.sendAT("+CPSI?");
    String response;
    modem.waitResponse(60000UL, response);
    payload.concat(response);

    // AT+SMPUB=<topic>,<content length>,<qos>,<retain><CR>message is entered Quit edit mode if payload.length equals to <content length>
    snprintf(buffer, 1024, "+SMPUB=\"t/%s/%s\",%d,1,1", username, clientID, payload.length());
    modem.sendAT(buffer);
    if (modem.waitResponse(">") == 1) {
        modem.stream.write(payload.c_str(), payload.length());
        Serial.print("Try publish payload: ");
        Serial.println(payload);

        if (modem.waitResponse(3000)) {
            Serial.println("Send Packet success!");
        } else {
            Serial.println("Send Packet failed!");
        }
    }

    // Disconnect MQTT connection
    delay(3000);
    modem.sendAT("+SMDISC");
    if (modem.waitResponse() != 1) {
        Serial.println("Couden't disconnect");
        //return;
    }
}

void loopModem(){
    //todo:
    /*modem.sendAT("+CPSI?");
    //if (modem.waitResponse("+SMSTATE: ")) {}
    modem.waitResponse();
    sleep(60000);*/
}

#else
void setupModem(){
}
#endif
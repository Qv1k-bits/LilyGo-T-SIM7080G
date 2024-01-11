/**
 * @file      main.ino
 * @author    Kristian van Kints
 * @license   
 * @copyright Copyright (c)
 * @date      2024-01-09
 *
 */
#include <Arduino.h>
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#define TINY_GSM_RX_BUFFER 1024
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;
#include "utilities.h"
#include "settings.h"
#include "./certs/EMQX_root_CA.h"

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define randMax 35
#define randMin 18
char buffer[1024] = {0};
bool level = false;
bool send_flag = true;

bool isConnect()
{
    modem.sendAT("+SMSTATE?");
    if (modem.waitResponse("+SMSTATE: ")) {
        String res = modem.stream.readStringUntil('\r');
        return res.toInt();
    }
    return false;
}

// retrieve the Power Saving Mode (PSM) timer value from the modem
void getPsmTimer()
{
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
void writeCaFiles(int index, const char *filename, const char *data,
                  size_t lenght){
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

void test_modem(){
    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 6) {
            // Pull down PWRKEY for more than 1 second according to manual
            // requirements
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retry = 0;
            Serial.println("Retry start modem .");
        }
    }
    Serial.println();
    Serial.println("Modem started!");
}

void MQTT_connect(){
    /*********************************
    * step 2 : start modem
    ***********************************/
    test_modem();
    /*********************************
     * step 3 : Check if the SIM card is inserted
     ***********************************/
    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("SIM Card is not insert!!!");
        return;
    }

    // Disable RF
    modem.sendAT("+CFUN=0");
    if (modem.waitResponse(20000UL) != 1) {
        Serial.println("Disable RF Failed!");
    }
    /*********************************
     * step 4 : Set the network mode to NB-IOT
     ***********************************/
    modem.setNetworkMode(3);  // use automatic
    modem.setPreferredMode(MODEM_CATM_NBIOT);
    uint8_t pre = modem.getPreferredMode();
    uint8_t mode = modem.getNetworkMode();
    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);

    //Set the APN manually. Some operators need to set APN first when registering the network.
    modem.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Set operators apn Failed!");
        return;
    }

    // Enable RF
    modem.sendAT("+CFUN=1");
    if (modem.waitResponse(20000UL) != 1) {
        Serial.println("Enable RF Failed!");
    }
    /*********************************
     * step 5 : Wait for the network registration to succeed
     ***********************************/
    RegStatus s;
    do {
        s = modem.getRegistrationStatus();
        if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
            Serial.print(".");
            delay(1000);
        }

    } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

    Serial.println();
    Serial.print("Network register info:");
    Serial.println(register_info[s]);

    // Activate network bearer, APN can not be configured by default,
    // if the SIM card is locked, please configure the correct APN and user
    // password, use the gprsConnect() method

    //!! Set the APN manually. Some operators need to set APN first when registering the network.
    modem.sendAT("+CNCFG=0,1,\"", apn, "\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Config apn Failed!");
        return;
    }

    modem.sendAT("+CNACT=0,1");
    if (modem.waitResponse() != 1) {
        Serial.println("Activate network bearer Failed!");
        return;
    }
    // if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    //     return ;
    // }

    bool res = modem.isGprsConnected();
    Serial.print("GPRS status:");
    Serial.println(res ? "connected" : "not connected");

    // Enable Local Time Stamp for getting network time
    modem.sendAT(GF("+CLTS=1"));
    if (modem.waitResponse(10000L) != 1) {
        Serial.println("Enable Local Time Stamp Failed!");
        return;
    }

    // Serial.println("Watiing time sync.");
    // while (modem.waitResponse("*PSUTTZ:") != 1) {
    //     Serial.print(".");
    // }
    // Serial.println();

    // Before connecting, you need to confirm that the time has been synchronized.
    modem.sendAT("+CCLK?");
    modem.waitResponse(30000);
    /*********************************
     * step 6 : import  ca
     ***********************************/
    //writeCaFiles(3, "rootCA.pem", root_CA, strlen(root_CA));// CA catificate from EMQX
    /*********************************
     * step 7 : setup MQTT Client
     ***********************************/
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
        return;
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

    modem.sendAT("+SMCONN");
    int8_t ret = modem.waitResponse(30000);
    if (ret != 1) {
        Serial.println("Connect failed, retry connect ...");
        delay(1000);
        return;
    } else {
        Serial.println("MQTT Client connected!");
    }
}

void setup(){
    Serial.begin(115200);
    Serial.println("................Setup.................");
    // Start while waiting for Serial monitoring
    while (!Serial);
    delay(3000);
    Serial.println();

    /***********************************
     *  step 1 : Initialize power chip, turn on modem and gps antenna power channel
     ***********************************/
    Serial.println("................Step 1................");
    Serial.println("Start to initialize power chip, turn on modem and gps antenna power channel ");
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL))
    {
        Serial.println("Failed to initialize power.....");
        while (1)
        {
            delay(5000);
        }
    }
    // Set the working voltage of the modem, please do not modify the parameters
    PMU.setDC3Voltage(3000); // SIM7080 Modem main power channel 2700~ 3400V
    PMU.enableDC3();

    // Modem GPS Power channel
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2(); // The antenna power must be turned on to use the GPS function

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();

    Serial.println("Step 1 done !");

    /*********************************
     * step 2 : start modem
     ***********************************/
    Serial.println("................Step 2................");
    Serial.println("Start to initialize T-SIM7080G modem now ");
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);

    test_modem();

    Serial.println("T-SIM7080G modem well started!");
    Serial.println("Step 2 done !");

    /***********************************
     * step 3 : Check if the SIM card is inserted
     ***********************************/
    Serial.println("................Step 3................");
    Serial.println("Start to check if the SIM card is inserted ");

    String result;

    if (modem.getSimStatus() != SIM_READY)
    {
        Serial.println("SIM Card is not insert!!!");
        return;
    }
    else
    {
        Serial.println("SIM Card is insert !");
    }
    Serial.println("Step 3 done !");

    
    /*********************************
     * step 4 : Set the network mode to NB-IOT
     ***********************************/
    Serial.println("................Step 4................");
    Serial.println("Start to set the network mode to NB-IOT ");
    modem.setNetworkMode(2);  // use automatic
    modem.setPreferredMode(MODEM_NB_IOT);
    uint8_t pre = modem.getPreferredMode();
    uint8_t mode = modem.getNetworkMode();
    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);

    Serial.println("Step 4 done !");

    /***********************************
     * step 5 : Start network registration
     ***********************************/
    Serial.println("................Step 5................");
    Serial.println("Start to perform network registration, configure APN and ping 8.8.8.8");

    // Important !
    // To use AT&T NB-IOT network, you must correctly configure as below ATT NB-IoT OneRate APN "m2mNB16.com.attz",
    // Otherwise ATT will assign a general APN like "m2mglobal" which seems blocks 443,8883,8884 ports.
    Serial.println("Configuring APN...");
    modem.sendAT("+CGDCONT=1,\"IP\",\"lpwa.telia.iot\"");
    //modem.sendAT("+CGDCONT=1,\"IP\",\"iot.1nce.net\"");
    modem.waitResponse();

    RegStatus s;
    do
    {
        s = modem.getRegistrationStatus();
        if (s != REG_OK_HOME && s != REG_OK_ROAMING)
        {
            Serial.print(".");
            PMU.setChargingLedMode(level ? XPOWERS_CHG_LED_ON : XPOWERS_CHG_LED_OFF);
            level ^= 1;
            delay(1000);
        }

    } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

    Serial.println();
    Serial.print("Network register info:");
    Serial.println(register_info[s]);

    modem.sendAT("+CGNAPN");
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("Get APN Failed!");
        return;
    }

    Serial.println("Step 5 done !");

    /***********************************
     * step 6 : Check if the network bearer is activated
     ***********************************/
    Serial.println("................Step 6................");
    Serial.println("Start to activate the network bearer");

    // Check the status of the network bearer
    Serial.println("Checking the status of network bearer ...");
    modem.sendAT("+CNACT?"); // Send the AT command to query the network bearer status
    String response;
    int8_t ret = modem.waitResponse(10000UL, response); // Wait for the response with a 10-second timeout

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
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("Ping Failed!");
        return;
    }
    else
    {
        Serial.println(response);
    }

    Serial.println("Step 6 done !");

    /***********************************
     * step 7 : Get the modem, SIM and network information
     ***********************************/
    Serial.println("................Step 7................");
    Serial.println("show the information of the Modem, SIM and network  ");

    Serial.println("T-SIM7080G Firmware Version: ");
    modem.sendAT("+CGMR");
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("Get Firmware Version Failed!");
    }
    else
    {
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
    Serial.println("Step 7 done !");

    /***********************************
     * step 8 : import  rootCA
     ***********************************/
    Serial.println("................Step 8................");
    Serial.println("start to write the root CA, device certificate and device private key to the modem ");

    writeCaFiles(3, "rootCA.pem", root_CA, strlen(root_CA));                 // root_CA is retrieved from Mosquitto_root_CA.h, which is downloaded from https://test.mosquitto.org/ssl/mosquitto.org.crt
    //writeCaFiles(3, "deviceCert.crt", Client_CRT, strlen(Client_CRT));       // Client_CRT is retrieved from Mosquitto_Client_CRT.h, please follow the guide to generate the device certificate in https://test.mosquitto.org/ssl/
    //writeCaFiles(3, "devicePrivateKey.pem", Client_PSK, strlen(Client_PSK)); // Client_PSK is retrieved from Mosquitto_Client_PSK.h, please follow the guide to generate the device certificate and private key in https://test.mosquitto.org/ssl/

    Serial.println("Step 8 done !");

    /***********************************
     * step 9 : Setup the configuration of TLS/SSL
     ***********************************/
    Serial.println("................Step 9................");
    Serial.println("start to configure the TLS/SSL parameters ");
    
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

    Serial.println("Step 9 done !");

    /***********************************
     * step 10 : Start to connect the MQTT server
     ***********************************/
    Serial.println("................Step 10...............");
    Serial.println("start to connect the MQTT server ");

    Serial.println("Connecting to MQTT server ...");
    while (true)
    {
        modem.sendAT("+SMCONN");
        String response;
        ret = modem.waitResponse(60000UL, response);

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
    Serial.println("Step 10 done !");
    Serial.println("Setup done !");
}

void loop(){
    if (send_flag==true){
        MQTT_connect();
        send_flag = false;
    }
    Serial.println();
    // Publish fake data
    String payload = "";

    payload.concat(clientID);
    payload.concat(",");

    payload.concat(modem.getGSMDateTime(DATE_FULL));
    payload.concat(",");

    int csq = modem.getSignalQuality();
    Serial.print("Signal quality:");
    Serial.println(csq);
    payload.concat(csq);
    payload.concat(",");

    int temp =  rand() % (randMax - randMin) + randMin;
    payload.concat("temp,");
    payload.concat(temp);
    payload.concat("\r\n");

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

    if (!isConnect()) {
        Serial.println("MQTT Client disconnect!"); delay(1000);
        //return;
    }

    // Disconnect MQTT connection
    modem.sendAT("+SMDISC");
    if (modem.waitResponse() != 1) {
        Serial.println("Couden't disconnect");
        //return;
    }

    // Disable RF
    modem.sendAT("+CFUN=0");
    if (modem.waitResponse(20000UL) != 1) {
        Serial.println("Disable RF Failed!");
    }

    delay(180000);
}

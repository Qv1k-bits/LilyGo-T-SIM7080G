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
    //writeCaFiles(3, "server-ca.crt", root_CA, strlen(root_CA));// CA catificate from EMQX
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
    // AT+CSSLCFG="CONVERT",2,"server-ca.crt"
    modem.sendAT("+CSSLCFG=\"CONVERT\",2,\"server-ca.crt\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert server-ca.crt failed!");
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
    modem.sendAT("+SMSSL=1,\"server-ca.crt\",\"\"");
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

    // Start while waiting for Serial monitoring
    while (!Serial);
    delay(3000);
    Serial.println();

    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
     ***********************************/
    Serial.println("Step 1:");
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }
    // Set the working voltage of the modem, please do not modify the parameters
    PMU.setDC3Voltage(3000);  // SIM7080 Modem main power channel 2700~ 3400V
    PMU.enableDC3();
    // Modem GPS Power channel
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();  // The antenna power must be turned on to use the GPS
                        // function
    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();

    /*********************************
     * step 2 : start modem
     ***********************************/
    Serial.println("Step 2:");
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);

    test_modem();

    /*********************************
     * step 4 : Set the network mode to NB-IOT
     ***********************************/
    Serial.println("Step 4:");
    modem.setNetworkMode(2);  // use automatic
    modem.setPreferredMode(MODEM_CATM_NBIOT);
    uint8_t pre = modem.getPreferredMode();
    uint8_t mode = modem.getNetworkMode();
    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);

    /*********************************
     * step 6 : import  ca
     ***********************************/
    Serial.println("Step 6:");
    writeCaFiles(3, "server-ca.crt", root_CA, strlen(root_CA));// CA catificate from EMQX
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

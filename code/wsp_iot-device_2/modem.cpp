/**
 * @file      modem.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-21
 *
 */
// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#include "settings.h"

#if defined(USING_MODEM)
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(Serial1);
#endif

bool getLoaction();
void loactionTask(void *);
bool isConnect();
void getPsmTimer();
void writeCaFiles(int index, const char *filename, const char *data, size_t lenght);
void testModem();
void setupNBIoTNetwork(int MODEM_NB_IOT);

void setupModem(){
    Serial.println("Initializing modem...");
    
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);

    // digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    // delay(100);
    // digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    // delay(1000);
    // digitalWrite(BOARD_MODEM_PWR_PIN, LOW);

    testModem();

    Serial.print("Modem started!");

    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("SIM Card is not insert!!!");
        return ;
    }

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

    if (modem.enableGPS() == false) {
        Serial.println("Enable gps failed!");
    }

    xTaskCreate(loactionTask, "gps", 4096, NULL, 10, NULL);
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
    int retryCount = 0;
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
            retry = 0;
        }
    }
}

void setupNBIoTNetwork(int MODEM_NB_IOT){
    Serial.println("Start to set the network mode to NB-IOT ");
    modem.setNetworkMode(2);  // use automatic
    modem.setPreferredMode(MODEM_NB_IOT);
    uint8_t pre = modem.getPreferredMode();
    uint8_t mode = modem.getNetworkMode();
    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);
}

#else
void setupModem(){

}
#endif
















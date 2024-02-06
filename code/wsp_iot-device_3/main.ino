/**
 * @file      AllFunction.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#include "power.h"
#include "utilities.h"
#include "sdcard.h"
#include "modem.h"
//#include "settings.h"

#define setupDelay 10

void loopPeripherals(void *ptr);
void getWakeupReason();

void setup(){
    bool ret = false;

    Serial.begin(115200);
    // Start while waiting for Serial monitoring
    while (!Serial);
    delay(3000);

    Serial.println();
    Serial.println("===============Start setup===============");
    Serial.println();
    delay(setupDelay);

    /*Serial.println("=========================================");
    Serial.println("Start Wake up reason");
    getWakeupReason();
    Serial.println();
    delay(setupDelay);*/

    /*Serial.println("=========================================");
    Serial.println("Check PSRAM");
    if (!psramFound()) {
        Serial.println("ERROR:PSRAM not found !");
    }
    Serial.println();
    delay(setupDelay);*/

    Serial.println("=========================================");
    Serial.println("Start Power setup");
    setupPower();
    Serial.println();
    delay(setupDelay);

    /*Serial.println("=========================================");
    Serial.println("Start SDcard setup");
    setupSdcard();
    Serial.println();
    delay(setupDelay);*/

    Serial.println("=========================================");
    Serial.println("Start Modem setup");
    setupModem();
    Serial.println();
    delay(setupDelay);

    Serial.println("=========================================");
    Serial.println("Show modem information");
    showModemInfo();
    Serial.println();
    delay(setupDelay);

    Serial.println("=========================================");
    Serial.println("Start writing certificates");
    writeCerts();
    Serial.println();
    delay(setupDelay);

    /*Serial.println("=========================================");
    Serial.println("Start TLS/SSL parameter setup");
    setup_TLS_SSL();
    Serial.println();
    delay(setupDelay);*/

    /*Serial.println("=========================================");
    //Serial.println("Connecting MQTT");
    mqttConnect();
    Serial.println();
    delay(setupDelay);*/

    Serial.println("=========================================");
    Serial.println("Create loopPeripherals task");
    //xTaskCreate(loopPeripherals, "App/per", 4 * 1024, NULL, 8, NULL);
    Serial.println();
    delay(setupDelay);

    Serial.println("===============Setup ran !===============");
}

void loop(){
    String status;
    battStatus(status);
    publishMsg(status);
    delay(180000UL);
}

void getWakeupReason(){
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        //!< In case of deep sleep, reset was not caused by exit from deep sleep
        Serial.println("In case of deep sleep, reset was not caused by exit from deep sleep");
        break;
    case ESP_SLEEP_WAKEUP_ALL:
        //!< Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source
        Serial.println("Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source");
        break;
    case ESP_SLEEP_WAKEUP_EXT0:
        //!< Wakeup caused by external signal using RTC_IO
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        //!< Wakeup caused by external signal using RTC_CNTL
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        //!< Wakeup caused by timer
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        //!< Wakeup caused by touchpad
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        //!< Wakeup caused by ULP program
        Serial.println("Wakeup caused by ULP program");
        break;
    case  ESP_SLEEP_WAKEUP_GPIO:
        //!< Wakeup caused by GPIO (light sleep only)
        Serial.println("Wakeup caused by GPIO (light sleep only)");
        break;
    case ESP_SLEEP_WAKEUP_UART:
        //!< Wakeup caused by UART (light sleep only)
        Serial.println("Wakeup caused by UART (light sleep only)");
        break;
    case ESP_SLEEP_WAKEUP_WIFI:
        //!< Wakeup caused by WIFI (light sleep only)
        Serial.println("Wakeup caused by WIFI (light sleep only)");
        break;
    case ESP_SLEEP_WAKEUP_COCPU:
        //!< Wakeup caused by COCPU int
        Serial.println("Wakeup caused by COCPU int");
        break;
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
        //!< Wakeup caused by COCPU crash
        Serial.println("Wakeup caused by COCPU crash");
        break;
    case  ESP_SLEEP_WAKEUP_BT:
        //!< Wakeup caused by BT (light sleep only)
        Serial.println("Wakeup caused by BT (light sleep only)");
        break;
    default :
        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
        break;

    }
}

void loopPeripherals(void *ptr){
    while (1){
        //loopPower();
        loopModem();
        delay(8);
    }
}

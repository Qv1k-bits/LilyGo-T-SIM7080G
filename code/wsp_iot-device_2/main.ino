/**
 * @file      AllFunction.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
//#include "camera.h"
#include "power.h"
//#include "network.h"
//#include "server.h"
#include "utilities.h"
//#include "esp_camera.h"
#include "sdcard.h"
#include "modem.h"
#include "settings.h"

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

    Serial.println("=========================================");
    Serial.println("Start Wake up reason");
    getWakeupReason();

    Serial.println("=========================================");
    Serial.println("Check PSRAM");
    if (!psramFound()) {
        Serial.println("ERROR:PSRAM not found !");
    }

    Serial.println("=========================================");
    Serial.println("Start setup Power");
    setupPower();

    Serial.println("=========================================");
    Serial.println("Start setup SDcard");
    setupSdcard();

    Serial.println("=========================================");
    Serial.println("Start setup Modem");
    setupModem();

    Serial.println("=========================================");
    Serial.println("Start setup modem mode");
    setupNBIoTNetwork(MODEM_NB_IOT);

    Serial.println("=========================================");
    Serial.println("Create loopPeripherals task");
    //xTaskCreate(loopPeripherals, "App/per", 4 * 1024, NULL, 8, NULL);

    Serial.println("===============Setup ran !===============");
}

void loop(){
    delay(1000);
}

void getWakeupReason()
{
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
        loopPower();
        delay(8);
    }
}

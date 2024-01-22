/**
 * @file      modem.h
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-21
 *
 */

#pragma once


void setupModem();
bool getLoaction();
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

void loopModem();
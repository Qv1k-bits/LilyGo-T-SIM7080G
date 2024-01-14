/**
 * @file      utilities.h
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */

#pragma once

// ===================
// Select camera model
// ===================
// #define EMQX_public
#define EMQX

#if defined(EMQX)
//!! Set the APN manually. Some operators need to set APN first when registering the network.
//!! Set the APN manually. Some operators need to set APN first when registering the network.
//!! Set the APN manually. Some operators need to set APN first when registering the network.
// Using 7080G with Hologram.io , https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/issues/19
//#define apn         "hologram"
#define apn             ("lpwa.telia.iot")
#define gprsUser        ("")
#define gprsPass        ("")
//  server address and port
#define server          ("kaf52e8a.ala.us-east-1.emqxsl.com")
#define port            (8883)
#define username        ("telia")
#define password        ("teliatest")
#define clientID        ("SIM7080G")
#define data_channel    (0)

#elif defined(EMQX_public)
//!! Set the APN manually. Some operators need to set APN first when registering the network.
//!! Set the APN manually. Some operators need to set APN first when registering the network.
//!! Set the APN manually. Some operators need to set APN first when registering the network.
// Using 7080G with Hologram.io , https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/issues/19
//#define apn         "hologram"
#define apn             ("lpwa.telia.iot")
#define gprsUser        ("")
#define gprsPass        ("")
//  server address and port
#define server          ("broker.emqx.io")
#define port            (8883)
#define username        ("emqx")
#define password        ("public")
#define clientID        ("SIM7080G")
#define data_channel    (0)

#else
#error "Camera model not selected"
#endif

/*const char *register_info[] = {
    "Not registered, MT is not currently searching an operator to register "
    "to.The GPRS service is disabled, the UE is allowed to attach for GPRS if "
    "requested by the user.",
    "Registered, home network.",
    "Not registered, but MT is currently trying to attach or searching an "
    "operator to register to. The GPRS service is enabled, but an allowable "
    "PLMN is currently not available. The UE will start a GPRS attach as soon "
    "as an allowable PLMN is available.",
    "Registration denied, The GPRS service is disabled, the UE is not allowed "
    "to attach for GPRS if it is requested by the user.",
    "Unknown.",
    "Registered, roaming.",
};*/

enum {
    MODEM_CATM = 1,
    MODEM_NB_IOT,
    MODEM_CATM_NBIOT,
};
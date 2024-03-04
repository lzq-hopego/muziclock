#ifndef __NET_H
#define __NET_H
#include "common.h"

void wifiConfigBySoftAP(void);
void doClient(void);
void connectWiFi(int timeOut_s);
void getNowWeather(void);
void getFutureWeather(void);
void getNTPTime(void);
void checkWiFiStatus(void);
void restartSystem(String msg, bool endTips);
extern bool queryNowWeatherSuccess;
extern bool queryFutureWeatherSuccess;
extern String ssid;
extern String pass;
extern String city;
extern bool isStartQuery;

#endif
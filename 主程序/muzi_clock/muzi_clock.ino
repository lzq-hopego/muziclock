#include <OneButton.h>
#include "net.h"
#include "common.h"
#include "PreferencesUtil.h"
#include "tftUtil.h"
#include "task.h"
#include <WiFi.h>
#include <ArduinoJson.h>

unsigned int prevDisplay = 0; // 实况天气页面上次显示的时间
unsigned int preTimerDisplay = 0; // 计数器页面上次显示的毫秒数/10,即10毫秒显示一次
unsigned long startMillis = 0; // 开始计数时的毫秒数
int synDataRestartTime = 60; // 同步NTP时间和天气信息时，超过多少秒就重启系统，防止网络不好时，傻等
bool isCouting = false; // 计时器是否正在工作

char incomingPacket[255];  //存储Udp客户端发过来的数据
WiFiUDP Udp;
String ip;

OneButton myButton(BUTTON, true);

void setup() {
  Serial.begin(115200);
  // TFT初始化
  tftInit();
  // 显示系统启动文字
  drawText("系统启动中...");
  // 测试的时候，先写入WiFi信息，省的配网，生产环境请注释掉
  // setInfo4Test();
  // 查询是否有配置过Wifi，没有->进入Wifi配置页面（0），有->进入天气时钟页面（1）
  getWiFiCity();
  // nvs中没有WiFi信息，进入配置页面
  if(ssid.length() == 0 || pass.length() == 0 || city.length() == 0 ){
    currentPage = SETTING; // 将页面置为配置页面initDatas
    wifiConfigBySoftAP(); // 开启SoftAP配置WiFi  
  }else{ // 有WiFi信息，连接WiFi后进入时钟页面
    currentPage = WEATHER; // 将页面置为时钟页面
    // 连接WiFi,30秒超时重启并恢复出厂设置
    connectWiFi(30); 
    // 初始化一些列数据:NTP对时、实况天气、一周天气
    initDatas();
    // 绘制实况天气页面
    drawWeatherPage();
    // 多任务启动
    startRunner();
    // 初始化定时器，让查询天气的多线程任务在一小时后再使能
    startTimerQueryWeather();
    // 初始化按键监控
    myButton.attachClick(click);
    myButton.attachDoubleClick(doubleclick);
    myButton.attachLongPressStart(longclick);
    myButton.setPressMs(2000); //设置长按时间
    // myButton.setClickMs(300); //设置单击时间
    myButton.setDebounceMs(10); //设置消抖时长 
  }

Udp.begin(1122);//在1122端口上监听udp的消息
ip=WiFi.localIP().toString();//获取当前ip
}

void loop() {
  /*接收发送过来的Udp数据*/
  int Data_length = Udp.parsePacket();  //获取接收的数据的长度
  if (Data_length)                      //如果有数据那么Data_length不为0，无数据Data_length为0
  {
    int len = Udp.read(incomingPacket, 255);  //读取数据，将数据保存在数组incomingPacket中
    if (len > 0)                              //为了避免获取的数据后面乱码做的判断
    {
      incomingPacket[len] = 0;
    }
    
    if(currentPage==CPU){
      Serial.println(incomingPacket);
      StaticJsonDocument<1024> dict; //声明一个静态JsonDocument对象
      DeserializationError error = deserializeJson(dict, String(incomingPacket)); //反序列化JSON数据
      if(!error){
        if(dict["command"]=="theme"){
          if(backColor == BACK_BLACK){ // 确认当前的背景颜色
              udpsend("back");
            }else{
              udpsend("white");
            }
        }else if(dict["command"]=="restart"){
          Serial.println("恢复出厂设置");
          udpsend("ok");
          // 恢复出厂设置并重启
          clearWiFiCity();
          restartSystem("已恢复出厂设置", false);
        }else if(dict["command"]=="changetheme"){
          udpsend("ok");
          if(backColor == BACK_BLACK){ // 原先为黑色主题，改为白色
            backColor = BACK_WHITE;
            backFillColor = 0xFFFF;
            penColor = 0x0000;
          }else{
            backColor = BACK_BLACK;
            backFillColor = 0x0000;
            penColor = 0xFFFF;
          }
          setBackColor(backColor);
          drawCpuPage(ip);
        }else if(dict["command"]=="changesetting"){
          if(dict["city"]){
            if(dict["ssid"]){
              if(dict["pass"]){
                udpsend("ok");
                setWiFiCity(dict["ssid"],dict["pass"],dict["city"]);
                restartSystem("已设置", false);
              }
            }else{
              udpsend("ok");
              setWiFiCity(ssid,pass,dict["city"]);
              restartSystem("已设置", false);
            }
          }else{
            udpsend("bad");
          }
        }else if(dict["command"]=="setting"){
          String stringtext;
          if(backColor == BACK_BLACK){ //判断主体
            stringtext="{'ssid':'"+ssid+"','pass':'"+pass+"','city':'"+city+"','theme':'back'}";
          }else{
            stringtext="{'ssid':'"+ssid+"','pass':'"+pass+"','city':'"+city+"','theme':'white'}";
          }
          udpsend(stringtext);
        }
      drawCpuPageText(dict["cpu"],dict["mem"],dict["net"],Udp.remoteIP().toString());
      //  tft.pushImage(60, 210, 120, 96,dict["img"]);
      }
      
    }
  }
  myButton.tick();
  switch(currentPage){
    case SETTING:  // 配置页面
      doClient(); // 监听客户端配网请求
      break;
    case WEATHER:  // 天气时钟页面
      executeRunner();
      time_t now;
      time(&now);
      if(now != prevDisplay){ // 每秒更新一次时间显示
        prevDisplay = now;
        // 绘制时间、日期、星期
        drawDateWeek();
      }
      break;
    case FUTUREWEATHER:  // 未来天气页面
      executeRunner();
      break;
    case THEME:  // 黑白主题设置页面
      executeRunner();
      break;
    case TIMER:  // 计时器页面
      if(isCouting && (millis() / 10) != preTimerDisplay){ // 每十毫秒更新一次数字显示
        preTimerDisplay = millis() / 10;
        // 绘制计数器数字
        drawNumsByCount(timerCount + millis() - startMillis);
      }
      break;
    case RESET:  // 恢复出厂设置页面
      executeRunner();
      break;
    case CPU:   //测试页面
      executeRunner();
      break;
    default:
      break;
  }
}
////////////////////////// 按键区///////////////////////
// 单击操作，用来切换各个页面
void click(){
  if(currentPage == TIMER){
    if(!isCouting){
      // Serial.println("开始计数");
      startMillis = millis();
    }else{
      // Serial.println("停止计数");
      timerCount += millis() - startMillis;
      // 绘制计数器数字
      drawNumsByCount(timerCount);
    }
    isCouting = !isCouting;
  }
  // else if(currentPage == SETTING){
  //   restartSystem("", true);
  // }
  Serial.println("单击了按钮");
}
void doubleclick(){
  switch(currentPage){
    case WEATHER:
      disableAnimScrollText();
      drawFutureWeatherPage();
      currentPage = FUTUREWEATHER;
      break;
    case FUTUREWEATHER:
      drawThemePage();
      currentPage = THEME;
      break;
    case THEME:
      drawTimerPage();
      currentPage = TIMER;
      break;
    case TIMER:
      drawResetPage();
      currentPage = RESET;
      break;
    case RESET:
      drawCpuPage(ip);
      currentPage = CPU;
      break;
    case CPU:
      drawWeatherPage();
      enableAnimScrollText();
      currentPage = WEATHER;
      break;
    default:
      break;
  }
}
void longclick(){
  if(currentPage == RESET){
    Serial.println("恢复出厂设置");
    // 恢复出厂设置并重启
    clearWiFiCity();
    restartSystem("已恢复出厂设置", false);
  }else if(currentPage == THEME){
    Serial.println("更改主题");
    if(backColor == BACK_BLACK){ // 原先为黑色主题，改为白色
      backColor = BACK_WHITE;
      backFillColor = 0xFFFF;
      penColor = 0x0000;
    }else{
      backColor = BACK_BLACK;
      backFillColor = 0x0000;
      penColor = 0xFFFF;
    }
    // 将新的主题存入nvs
    setBackColor(backColor);
    // 返回实时天气页面
    drawWeatherPage();
    enableAnimScrollText();
    currentPage = WEATHER;
  }else if(currentPage == TIMER){
    Serial.println("计数器归零");
    timerCount = 0; // 计数值归零
    isCouting = false; // 计数器标志位置为false
    drawNumsByCount(timerCount); // 重新绘制计数区域，提示区域不用变
  }else if(currentPage == WEATHER){
    restartSystem("重启", true);  //主页长按重启
  }
}
////////////////////////////////////////////////////////


// 初始化一些列数据:NTP对时、实况天气、一周天气
void initDatas(){
  startTimerShowTips(); // 获取数据时，循环显示提示文字
  // 记录此时的时间，在同步数据时，超过一定的时间，就直接重启
  time_t start;
  time(&start);
  // 获取NTP并同步至RTC,第一次同步失败，就一直尝试同步
  getNTPTime();
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
    time_t end;
    time(&end);
    if((end - start) > synDataRestartTime){
      restartSystem("同步数据失败", true);
    }
    Serial.println("时钟对时失败...");
    getNTPTime();
  }
  //第一次查询实况天气,如果查询失败，就一直反复查询
  getNowWeather();
  while(!queryNowWeatherSuccess){
    time_t end;
    time(&end);
    if((end - start) > synDataRestartTime){
      restartSystem("同步数据失败", true);
    }
    getNowWeather();
  }
  //第一次查询一周天气,如果查询失败，就一直反复查询
  getFutureWeather();
  while(!queryFutureWeatherSuccess){
    time_t end;
    time(&end);
    if((end - start) > synDataRestartTime){
      restartSystem("同步数据失败", true);
    }
    getFutureWeather();
  }
  //结束循环显示提示文字的定时器
  timerEnd(timerShowTips);
  //将isStartQuery置为false,告诉系统，启动时查询天气已完成
  isStartQuery = false;
}


void udpsend(String text){
  /*将接受到的数据发送回去*/
  Udp.beginPacket(Udp.remoteIP(),Udp.remotePort());  //准备发送数据到目标IP和目标端口
  Udp.print(text);  //将数据white放入发送的缓冲区        
  Udp.endPacket();  //向目标IP目标端口发送数据
}
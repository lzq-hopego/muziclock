#include <HTTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "common.h"
#include "PreferencesUtil.h"
#include "tftUtil.h"
#include "task.h"

void sendNTPpacket(IPAddress &address);
void startAP();
void startServer();
void scanWiFi();
void handleNotFound();
void handleRoot();
void handleConfigWifi();
void restartSystem(String msg, bool endTips);

bool queryNowWeatherSuccess = false;
bool queryFutureWeatherSuccess = false;
// Wifi相关
String ssid;  //WIFI名称
String pass;  //WIFI密码
String city;  // 城市
String WifiNames; // 根据搜索到的wifi生成的option字符串
// SoftAP相关
const char *APssid = "MuziClock";
IPAddress staticIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);
// 查询天气超时时间(ms)
int queryTimeout = 3000;
// 是否是刚启动时查询天气
bool isStartQuery = true; 

// 开启SoftAP进行配网
void wifiConfigBySoftAP(){
  // 开启AP模式，如果开启失败，重启系统
  startAP();
  // 扫描WiFi,并将扫描到的WiFi组成option选项字符串
  scanWiFi();
  // 启动服务器
  startServer();
  // 显示配置网络页面
  tft.pushImage(0, 0, 240, 320, QRcode);
}

// 处理服务器请求
void doClient(){
  server.handleClient();
}
// 处理404情况的函数'handleNotFound'
void handleNotFound(){
  handleRoot();//访问不存在目录则返回配置页面
}
// 处理网站根目录的访问请求
void handleRoot(){
  server.send(200,"text/html", ROOT_HTML_PAGE1 + WifiNames + ROOT_HTML_PAGE2);
}
// 提交数据后的提示页面
void handleConfigWifi(){
  //判断是否有WiFi名称
  if (server.hasArg("ssid")){
    Serial.print("获得WiFi名称:");
    ssid = server.arg("ssid");
    Serial.println(ssid);
  }else{
    Serial.println("错误, 没有发现WiFi名称");
    server.send(200, "text/html", "<meta charset='UTF-8'>错误, 没有发现WiFi名称");
    return;
  }
  //判断是否有WiFi密码
  if (server.hasArg("pass")){
    Serial.print("获得WiFi密码:");
    pass = server.arg("pass");
    Serial.println(pass);
  }else{
    Serial.println("错误, 没有发现WiFi密码");
    server.send(200, "text/html", "<meta charset='UTF-8'>错误, 没有发现WiFi密码");
    return;
  }
  //判断是否有city名称
  if (server.hasArg("city")){
    Serial.print("获得城市:");
    city = server.arg("city");
    Serial.println(city);
  }else{
    Serial.println("错误, 没有发现城市名称");
    server.send(200, "text/html", "<meta charset='UTF-8'>错误, 没有发现城市名称");
    return;
  }
  // 将信息存入nvs中
  setWiFiCity(ssid, pass, city);
  // 获得了所需要的一切信息，给客户端回复
  server.send(200, "text/html", "<meta charset='UTF-8'><style type='text/css'>body {font-size: 2rem;}</style><br/><br/>WiFi: " + ssid + "<br/>密码: " + pass + "<br/>城市: " + city + "<br/>已取得相关信息,正在尝试连接,请手动关闭此页面。");
  restartSystem("即将尝试连接", false);
}

// 连接WiFi
void connectWiFi(int timeOut_s){
  delay(1500); // 让“系统启动中”字样多显示一会
  drawText("正在连接网络...");
  int connectTime = 0; //用于连接计时，如果长时间连接不成功，复位设备
  pinMode(D4,OUTPUT);
  Serial.print("正在连接网络");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(D4, !digitalRead(D4));
    delay(500);
    connectTime++;
    if (connectTime > 2 * timeOut_s){ //长时间连接不上，清除nvs中保存的网络数据，并重启系统
      Serial.println("网络连接失败,即将重新开始配置网络...");
      clearWiFiCity();
      restartSystem("网络连接失败", false);
    }
  }
  digitalWrite(D4, LOW); // 连接成功后，将D4指示灯熄灭
  Serial.println("网络连接成功");
  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
}
// 检查WiFi连接状态，如果断开了，重新连接
void checkWiFiStatus(){
  if(WiFi.status() != WL_CONNECTED){ // 网络断开了，进行重连
    Serial.println("网络断开，即将重新连接...");
    WiFi.begin(ssid, pass);
  }
}

// 启动服务器
void startServer(){
  // 当浏览器请求服务器根目录(网站首页)时调用自定义函数handleRoot处理，设置主页回调函数，必须添加第二个参数HTTP_GET，否则无法强制门户
  server.on("/", HTTP_GET, handleRoot);
  // 当浏览器请求服务器/configwifi(表单字段)目录时调用自定义函数handleConfigWifi处理
  server.on("/configwifi", HTTP_POST, handleConfigWifi);
  // 当浏览器请求的网络资源无法在服务器找到时调用自定义函数handleNotFound处理   
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("服务器启动成功！");
}
// 开启AP模式，如果开启失败，重启系统
void startAP(){
  Serial.println("开启AP模式...");
  WiFi.enableAP(true); // 使能AP模式
  //传入参数静态IP地址,网关,掩码
  WiFi.softAPConfig(staticIP, gateway, subnet);
  if (!WiFi.softAP(APssid)) {
    Serial.println("AP模式启动失败");
    ESP.restart(); // Ap模式启动失败，重启系统
  }  
  Serial.println("AP模式启动成功");
  Serial.print("IP地址: ");
  Serial.println(WiFi.softAPIP());
}
// 扫描WiFi,并将扫描到的Wifi组成option选项字符串
void scanWiFi(){
  Serial.println("开始扫描WiFi");
  int n = WiFi.scanNetworks();
  if (n){
    Serial.print("扫描到");
    Serial.print(n);
    Serial.println("个WIFI");
    WifiNames = "";
    for (size_t i = 0; i < n; i++){
      int32_t rssi = WiFi.RSSI(i);
      String signalStrength;
      if(rssi >= -35){
        signalStrength = " (信号极强)";
      }else if(rssi >= -50){
        signalStrength = " (信号强)";
      }else if(rssi >= -70){
        signalStrength = " (信号中)";
      }else{
        signalStrength = " (信号弱)";
      }
      WifiNames += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + signalStrength + "</option>";
      // Serial.print("WiFi的名称(SSID):");
      // Serial.println(WiFi.SSID(i));
    }
  }else{
    Serial.println("没扫描到WIFI");
  }
}

// 查询实况天气
void getNowWeather(){
  bool cityLegal = true; // 记录城市字符串是否合法
  queryNowWeatherSuccess = false; // 先置为false
  String url = nowURL + city;
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  httpClient.setConnectTimeout(queryTimeout);
  httpClient.begin(url);
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("获取成功");
    String str = httpClient.getString();
    Serial.println(str);
    if(str.indexOf("city不存在") > 0 || str.indexOf("errcode") > 0){ // 城市名称不合法
      cityLegal = false;
    }else{
      StaticJsonDocument<1024> doc; //声明一个静态JsonDocument对象
      DeserializationError error = deserializeJson(doc, str); //反序列化JSON数据
      if(!error){ //检查反序列化是否成功
        queryNowWeatherSuccess = true;
        //读取json节点
        nowWeather.city = doc["city"].as<const char*>();
        nowWeather.wea = doc["wea"].as<const char*>();
        nowWeather.wea_img = doc["wea_img"].as<const char*>();
        nowWeather.tem = doc["tem"].as<int>();
        nowWeather.tem_day = doc["tem_day"].as<int>();
        nowWeather.tem_night = doc["tem_night"].as<int>();
        String win = doc["win"];
        String win_speed = doc["win_speed"];
        nowWeather.win = win + win_speed;
        nowWeather.air = doc["air"].as<int>();
        int num;
        sscanf(doc["humidity"].as<const char*>(), "%d", &num); // 提取湿度中的数字，即把%过滤掉
        nowWeather.humidity = num;
      }
    }    
  } else {
    Serial.print("请求实况天气错误：");
    Serial.println(httpCode);
  }
  //关闭与服务器连接
  httpClient.end();
  if(!cityLegal && isStartQuery){ // 如果城市名称不对且是开机时第一次查询天气，则重启
    Serial.print("城市名称不合法，即将重启系统");
    clearWiFiCity(); // 清除配置好的信息
    restartSystem("城市名称无效", true);
  }
}
// 查询未来天气，经过实况天气一环，城市名称肯定是合法的，所以无需再检验
void getFutureWeather(){
  queryFutureWeatherSuccess = false; // 先置为false
  String url = futureURL + city;
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  httpClient.setConnectTimeout(queryTimeout);
  httpClient.begin(url);
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取一周天气数据");
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("获取成功");
    String str = httpClient.getString();
    Serial.println(str);
    StaticJsonDocument<2048> doc; //声明一个静态JsonDocument对象
    DeserializationError error = deserializeJson(doc, str); //反序列化JSON数据
    if(!error){ //检查反序列化是否成功
      queryFutureWeatherSuccess = true;
      //读取json节点
      futureWeather.city = doc["city"].as<const char*>();
      futureWeather.day0wea = doc["data"][0]["wea"].as<const char*>();
      futureWeather.day0wea_img = doc["data"][0]["wea_img"].as<const char*>();
      futureWeather.day0date = doc["data"][0]["date"].as<const char*>();
      futureWeather.day0tem_day = doc["data"][0]["tem_day"].as<int>();
      futureWeather.day0tem_night = doc["data"][0]["tem_night"].as<int>();
      futureWeather.day1wea = doc["data"][1]["wea"].as<const char*>();
      futureWeather.day1wea_img = doc["data"][1]["wea_img"].as<const char*>();
      futureWeather.day1date = doc["data"][1]["date"].as<const char*>();
      futureWeather.day1tem_day = doc["data"][1]["tem_day"].as<int>();
      futureWeather.day1tem_night = doc["data"][1]["tem_night"].as<int>();
      futureWeather.day2wea = doc["data"][2]["wea"].as<const char*>();
      futureWeather.day2wea_img = doc["data"][2]["wea_img"].as<const char*>();
      futureWeather.day2date = doc["data"][2]["date"].as<const char*>();
      futureWeather.day2tem_day = doc["data"][2]["tem_day"].as<int>();
      futureWeather.day2tem_night = doc["data"][2]["tem_night"].as<int>();
      futureWeather.day3wea = doc["data"][3]["wea"].as<const char*>();
      futureWeather.day3wea_img = doc["data"][3]["wea_img"].as<const char*>();
      futureWeather.day3date = doc["data"][3]["date"].as<const char*>();
      futureWeather.day3tem_day = doc["data"][3]["tem_day"].as<int>();
      futureWeather.day3tem_night = doc["data"][3]["tem_night"].as<int>();
      futureWeather.day4wea = doc["data"][4]["wea"].as<const char*>();
      futureWeather.day4wea_img = doc["data"][4]["wea_img"].as<const char*>();
      futureWeather.day4date = doc["data"][4]["date"].as<const char*>();
      futureWeather.day4tem_day = doc["data"][4]["tem_day"].as<int>();
      futureWeather.day4tem_night = doc["data"][4]["tem_night"].as<int>();
      futureWeather.day5wea = doc["data"][5]["wea"].as<const char*>();
      futureWeather.day5wea_img = doc["data"][5]["wea_img"].as<const char*>();
      futureWeather.day5date = doc["data"][5]["date"].as<const char*>();
      futureWeather.day5tem_day = doc["data"][5]["tem_day"].as<int>();
      futureWeather.day5tem_night = doc["data"][5]["tem_night"].as<int>();
      futureWeather.day6wea = doc["data"][6]["wea"].as<const char*>();
      futureWeather.day6wea_img = doc["data"][6]["wea_img"].as<const char*>();
      futureWeather.day6date = doc["data"][6]["date"].as<const char*>();
      futureWeather.day6tem_day = doc["data"][6]["tem_day"].as<int>();
      futureWeather.day6tem_night = doc["data"][6]["tem_night"].as<int>();
    }
  } else {
    Serial.print("请求一周天气错误：");
    Serial.println(httpCode);
  }
  //关闭与服务器连接
  httpClient.end();
}

// 获取NTP并同步RTC时间
void getNTPTime(){
  // 8 * 3600 东八区时间修正
  // 使用夏令时 daylightOffset_sec 就填写3600，否则就填写0；
  Serial.println("NTP对时...");
  configTime( 8 * 3600, 0, NTP1, NTP2, NTP3);
}
// 重启系统
// bool endTips  是否需要把“同步天气数据”文字的定时器任务取消
void restartSystem(String msg, bool endTips){
  if(endTips){
    //结束循环显示提示文字的定时器
    timerEnd(timerShowTips);
  }
  reflashTFT();
  for(int i = 3; i > 0; i--){
    String text = "";
    text = text + i + "秒后系统重启";
    draw2LineText(msg,text);
    delay(1000);
  }
  ESP.restart();
}


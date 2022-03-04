#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BC26_RESET_PIN A0
#define BC26_PWR_KEY 6
#define BC26_PSM_PIN 7
#define LED_RED A1
#define LED_GREEN A2
#define LED_BLUE 5
#define BUZZ 3
#define DEFAULT_TIMEOUT 10 //seconds
#define BUF_LEN1 300
#define BUF_LEN2 300
#define BC26_SUCC_RSP "OK"
#define BC26_AT_CHECK "AT\r\n"
#define BC26_ECHO_OFF "ATE0\r\n"
#define BC26_AT_CFUN "AT+CFUN=1,0\r\n"
#define BC26_AT_CPIN "AT+CPIN?\r\n"   //查询SIM卡是否就绪
#define BC26_AT_CEREG "AT+CEREG?\r\n" //查询模组网络注册状态
#define BC26_CEREG_RSP "+CEREG: 0,1"
#define BC26_CHECK_GPRS_ATTACH "AT+CGATT?\r\n" //查询是否成功联网
#define BC26_CGATT_SUCC_POSTFIX "+CGATT: 1"
#define BC26_ALI_PRESET "AT+QMTCFG=\"ALIAUTH\",0,\"%s\",\"%s\",\"%s\"\r\n"                  //配置MQTT的可选参数
#define BC26_ALI_OPEN_LINK "AT+QMTOPEN=0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\n" //为MQTT客户机打开一个网络
#define BC26_ALI_OPEN_LINK_SUCC "+QMTOPEN: 0,0"                                             //0：表示打开网络成功
#define BC26_ALI_BUILD_LINK "AT+QMTCONN=0,\"clientExample\"\r\n"                            //连接MQTT服务器
#define BC26_ALI_BUILD_LINK_SUCC "+QMTCONN: 0,0,0"
#define BC26_ALI_SENSOR_UPLOAD_SUCC "+QMTPUB: 0,12,0"
#define BC26_ALI_DISCON "AT+QMTDISC=0\r\n"
#define BC26_ALI_DISCON_SUCC "+QMTDISC: 0,0"

//上传
#define BC26_ALI_SENSOR_UPLOAD "AT+QMTPUB=0,12,1,0,\"/sys/%s/%s/thing/event/property/post\",\"{'id':'110','version':'1.0','method':'thing.event.property.post','params':{'temp':%d,'pres':%d,'humi':%d,'tempThreshold':%d,\"bmeAlarmState\":%d}}\"\r\n"

// #define BC26_ALI_SENSOR_UPLOAD "AT+QMTPUB=0,12,1,0,\"/sys/%s/%s/thing/event/property/post\",\"{'id':'110','version':'1.0','method':'thing.event.property.post','params':{'temp':%d,'pres':%d,'humi':%d,'tempThreshold':%d,'presThreshold':%d,'humiThreshold':%d,\"bmeAlarmState\":%d}}\"\r\n"

//下发
#define AT_QMT_SUB_tempThresholdChange "AT+QMTSUB=2,3,\"/sys/%s/%s/thing/service/tempThresholdChange\",0 \r\n"
#define AT_QMT_SUB_bmeAlarmFree "AT+QMTSUB=3,2,\"/sys/%s/%s/thing/service/bmeAlarmFree\",1 \r\n"
#define ALARM_FREE "bmeAlarmFree"

//三元组
#define ProductKey "gh7xzTmBItN"
#define DeviceName "Arduino_BME280"
#define DeviceSecret "0a9246dbc9165b6cb2f973dee13a6777"

Adafruit_BME280 bme;

int temp;
int pres;
int humi;
int tempThreshold;
int presThreshold;
int humiThreshold;
bool bmeAlarmStatus;
//const char *value_TH = "tempThreshold";

bool alarm_free; //警报解除
bool alarm_run;  //警报工作
bool flag_bmeAlarmFree = false;

char ATcmd[BUF_LEN1];
char ATbuffer[BUF_LEN2];


void setup();
void loop();
void getTemp();
void getPres();
void getHumi();
void alarm_work();
void alarm_off();
void alarm_on();
void BC26_init();
void BC26_reset();
bool BC26_network_check();
bool BC26_Ali_connect();
//bool sub_tempThresholdChange();
//bool sub_bmeAlarmFree();
bool BC26_sensor_upload(int temp,int pres,int humi,int tempThreshold,bool bmeAlarmState);
bool BC26_sensor_download();
//bool BC26_sensor_download_alarm();
bool check_send_cmd(const char *cmd, const char *resp, unsigned int timeout);
void cleanBuffer(char *buf, int len);
int ReadNum(const char *value);
void LED_init();

void setup()
{
  Serial.begin(115200);
  LED_init();
  BC26_init();
  alarm_off();
  while (1)
  {
    if (!BC26_network_check())
      continue;
    if (!BC26_Ali_connect())
      continue;
    break;
  }
  unsigned status;
  status = bme.begin();
  Serial.flush();
}

void loop()
{
  while (1)
  {
    getTemp();
    getPres();
    getHumi();
    Serial.print("tempThreshold->");
    Serial.println(tempThreshold);
    if (temp < tempThreshold)
    {
      bmeAlarmStatus = true;
      alarm_on();
      Serial.println("-> alarm !");
    }
    else
    {
      bmeAlarmStatus = false;
      alarm_off();
      Serial.println("-> work fine!");
    }
    BC26_sensor_upload(temp,pres,humi,tempThreshold,bmeAlarmStatus); 
    BC26_sensor_download();
    Serial.println("------------------------------------------------------------------");
  }
  LED_init();
  while (1);
}

void getTemp()
{
  temp = bme.readTemperature();
  Serial.print("temp->");
  Serial.print(temp);
  Serial.println(".C");
}

void getHumi()
{
  humi = bme.readHumidity();
  Serial.print("humi->");
  Serial.print(humi);
  Serial.println("%");
}

void getPres()
{
  pres = bme.readPressure()/100.0F;
  Serial.print("pres->");
  Serial.print(pres);
  Serial.println("hPa");
}


//void alarm_work()
//{
//  if (bmeAlarmStatus == true) //超过阈值
//  {
//    if (flag_bmeAlarmFree == true) //接收到解除警报
//    {
//      alarm_run = false;
//      flag_bmeAlarmFree = false; //仅关闭这一次，接受后复原flag_bmeAlarmFree
//      alarm_free = true;         //进行关闭警报
//      Serial.println("-> alarm free");
//    }
//    else
//    {
//      if (alarm_free == false) //没有接收过警报解除的指令
//      {
//        alarm_run = true;
//        Serial.println("-> alarm normal");
//      }
//    }
//    if (alarm_run == true)
//    {
//      alarm_on();
//      Serial.println("-> alarm !");
//    }
//    else
//    {
//      alarm_off();
//      Serial.println("-> alarm off");
//    }
//  }
//  else //正常范围
//  {
//    alarm_off();
//    Serial.println("-> work fine!");
//  }
//}

//------------------------------------------------------------

void alarm_off()
{
  digitalWrite(BUZZ, LOW);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
}

void alarm_on()
{
  digitalWrite(BUZZ, HIGH);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, LOW);
}

void BC26_init()
{
  pinMode(BC26_RESET_PIN, OUTPUT);
  pinMode(BC26_PWR_KEY, OUTPUT);
  pinMode(BC26_PSM_PIN, OUTPUT);
  digitalWrite(BC26_PWR_KEY, LOW);
  digitalWrite(BC26_PSM_PIN, LOW);
}

void BC26_reset()
{
  digitalWrite(BC26_RESET_PIN, HIGH);
  delay(100);
  digitalWrite(BC26_RESET_PIN, LOW);
  delay(20000);
  Serial.flush();
}

bool BC26_network_check()
{
  bool flag;

  flag = check_send_cmd(BC26_AT_CHECK, BC26_SUCC_RSP, DEFAULT_TIMEOUT);
  if (!flag)
    return false;
  flag = check_send_cmd(BC26_AT_CFUN, BC26_SUCC_RSP, DEFAULT_TIMEOUT);
  if (!flag)
    return false;

  //查询SIM卡是否准备就绪
  flag = check_send_cmd(BC26_AT_CPIN, BC26_SUCC_RSP, DEFAULT_TIMEOUT);
  if (!flag)
    return false;
  //查询模组是否已经成功注册网络
  flag = check_send_cmd(BC26_AT_CEREG, BC26_CEREG_RSP, DEFAULT_TIMEOUT);
  if (!flag)
    return false;
  //查询是否联网成功
  flag = check_send_cmd(BC26_CHECK_GPRS_ATTACH, BC26_CGATT_SUCC_POSTFIX, DEFAULT_TIMEOUT);
  if (flag)
  {
//    Serial.println("联网成功！");
    return flag;
  }
  else
  {
//    Serial.println("联网失败！");
    return false;
  }
}

bool BC26_Ali_connect()
{
  bool flag;
  cleanBuffer(ATcmd, BUF_LEN1);
  //配置MQTT可选参数
  snprintf(ATcmd, BUF_LEN1, BC26_ALI_PRESET, ProductKey, DeviceName, DeviceSecret); //发送设备三元组信息
  flag = check_send_cmd(ATcmd, BC26_SUCC_RSP, DEFAULT_TIMEOUT);
  if (!flag)
    return false;
  for (int count = 1; count <= 3; count++)
  {
    flag = check_send_cmd(BC26_ALI_OPEN_LINK, BC26_ALI_OPEN_LINK_SUCC, 40);
    if (flag)
      break;
  }
  if (!flag)
    return false;
  flag = check_send_cmd(BC26_ALI_BUILD_LINK, BC26_ALI_BUILD_LINK_SUCC, DEFAULT_TIMEOUT);
  if (flag)
  {
    Serial.println("BC26 online");
    return flag;
  }
  else
  {
    Serial.println("BC26 online failed");
    return false;
  }

  //订阅tempThresholdChange
  cleanBuffer(ATcmd, BUF_LEN1);
  snprintf(ATcmd, BUF_LEN1, AT_QMT_SUB_tempThresholdChange, ProductKey, DeviceName);
  flag = check_send_cmd(ATcmd, AT_QMT_SUB_tempThresholdChange, DEFAULT_TIMEOUT);
  Serial.println("Subscribing tempThresholdChange");

  //订阅bmeAlarmFree
//  cleanBuffer(ATcmd, BUF_LEN1);
//  snprintf(ATcmd, BUF_LEN1, AT_QMT_SUB_bmeAlarmFree, ProductKey, DeviceName);
//  flag = check_send_cmd(ATcmd, AT_QMT_SUB_bmeAlarmFree, DEFAULT_TIMEOUT);
//  Serial.println("Subscribing bmeAlarmFree");
  return flag;
}

//
//bool sub_tempThresholdChange(){
//    //订阅tempThresholdChange
//  bool flag;
//  cleanBuffer(ATcmd, BUF_LEN1);
//  snprintf(ATcmd, BUF_LEN1, AT_QMT_SUB_tempThresholdChange, ProductKey, DeviceName);
//  flag = check_send_cmd(ATcmd, AT_QMT_SUB_tempThresholdChange, DEFAULT_TIMEOUT);
//  Serial.println("sub_tempThresholdChange");
//  return flag;
//}
//
//
//bool sub_bmeAlarmFree(){
//    //订阅tempThresholdChange
//  bool flag;
//  cleanBuffer(ATcmd, BUF_LEN1);
//  snprintf(ATcmd, BUF_LEN1, AT_QMT_SUB_bmeAlarmFree, ProductKey, DeviceName);
//  flag = check_send_cmd(ATcmd, AT_QMT_SUB_bmeAlarmFree, DEFAULT_TIMEOUT);
//  Serial.println("sub_bmeAlarmFree");
//  return flag;
//}



//数据上传
bool BC26_sensor_upload(
    int temp,
    int pres,
    int humi,
    int tempThreshold,
    bool bmeAlarmState)
{
  bool flag;
  cleanBuffer(ATcmd, BUF_LEN1);
  snprintf(
      ATcmd,
      BUF_LEN1,
      BC26_ALI_SENSOR_UPLOAD,
      ProductKey,
      DeviceName,
      temp,
      pres,
      humi,
      tempThreshold,
      bmeAlarmState);
  flag = check_send_cmd(ATcmd, BC26_ALI_SENSOR_UPLOAD_SUCC, 20);
  return flag;
}

bool BC26_sensor_download()
{
  if (check_send_cmd("AT", "version", 1))
  {
    Serial.print("订阅返回: ");
    Serial.println(ATbuffer);
    int t = ReadNum("th");
    int t1;
    Serial.print("t->");
    Serial.println(t);
    if (t != 0)
    {
      tempThreshold = t;
      t1 = t;
    }
    if (t = 0)
    {
      tempThreshold = t1;
    }
    Serial.println("Refreshed Local tempThreshold: ");
    Serial.print(tempThreshold);
    Serial.println();
  }
  return false;
}

//bool BC26_sensor_download_alarm()
//{
//  if (check_send_cmd("check", ALARM_FREE, 1))
//  {
//    flag_bmeAlarmFree = true;
//  }
//  return false;
//}

bool check_send_cmd(const char *cmd, const char *resp, unsigned int timeout)
{
  int i = 0;
  unsigned long timeStart;
  timeStart = millis();
  cleanBuffer(ATbuffer, BUF_LEN2);
  Serial.println(cmd);

  while (1)
  {
    while (Serial.available())
    {
      ATbuffer[i++] = Serial.read();
      if (i >= BUF_LEN2)
        return false;
    }
    if (NULL != strstr(ATbuffer, resp))
      break;
    if ((unsigned long)(millis() - timeStart > timeout * 1000))
      break;
  }
  Serial.flush();

  if (NULL != strstr(ATbuffer, resp))
    return true;
  return false;
}

void cleanBuffer(char *buf, int len)
{
  for (int i = 0; i < len; i++)
  {
    buf[i] = '\0';
  }
}

int ReadNum(const char *value)
{
  char *str = strstr(ATbuffer, value);
  if (*str)
  {
    return atoi(&str[strlen(value) + 2]);
  }
  else
  {
    return -1;
  }
}

void LED_init()
{
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BUZZ, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(BUZZ, LOW);
}


//    alarm_work(); //判断是否报警
//    delay(1000);
  //  BC26_sensor_upload(temp,pres,humi,tempThreshold,presThreshold,humiThreshold,bmeAlarmStatus); 

  //  if (check_send_cmd("check", ALARM_FREE, 1))
//  {
//    flag_bmeAlarmFree = true;
//    Serial.print("订阅返回: ");
//    Serial.println(ATbuffer);
//  }
//  
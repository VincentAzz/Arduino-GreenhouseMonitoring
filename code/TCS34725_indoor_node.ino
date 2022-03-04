#include <Wire.h>
#include <Adafruit_TCS34725.h>

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
#define BC26_ALI_SENSOR_UPLOAD      "AT+QMTPUB=0,12,1,0,\"/sys/%s/%s/thing/event/property/post\",\"{'id':'110','version':'1.0','method':'thing.event.property.post','params':{'lux':%d,'luxThreshold':%d,'luxStatus':%d}}\"\r\n"
#define BC26_ALI_SUB_SET_t "AT+QMTSUB=1,12,\"/sys/%s/%s/thing/service/luxThresholdChange\",0 \r\n"
//#define AT_QMT_SUB_SET "AT+QMTSUB=0,2,\"/sys/%s/%s/thing/service/luxAlarmFree\",1 \r\n"

//#define ALARM_FREE "luxAlarmFree"

//三元组
#define ProductKey                  "gh7xwkp2FPl"
#define DeviceName                  "Arduino_TCS3472"
#define DeviceSecret                "ba11bacca19b6f69ada2379635e5d84e"


uint16_t r;                   //红
uint16_t g;                   //绿
uint16_t b;                   //蓝
uint16_t c;                   //未处理光照强度
uint16_t colorTemp;           //色温
int lux;                 //处理的光照度
int luxThreshold;  //光照强度的阈值
bool luxStatus;    

bool alarm_free; //警报解除
bool alarm_run;  //警报工作
bool flag_alarmFree = false;

char ATcmd[BUF_LEN1];
char ATbuffer[BUF_LEN2];

void isr(); 
void getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
void setup(); 
void loop(); 
//void alarm_work();
void alarm_off();
void alarm_on();
void BC26_init();
void BC26_reset();
bool BC26_network_check();
bool BC26_Ali_connect();
bool check_send_cmd(const char* cmd,const char* resp,unsigned int timeout);
void cleanBuffer(char *buf,int len);
int ReadNum(const char *value);
bool BC26_sensor_upload(int lux, int luxThreshold, bool luxStatus);
bool BC26_sensor_download();
//bool BC26_sensor_download_alarm();
void LED_init();


Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);
const int interruptPin = 2;
volatile boolean state = false;

void isr() 
{
  state = true;
}

//读取传感器原始数据
void getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c)
{
  *c = tcs.read16(TCS34725_CDATAL);
  *r = tcs.read16(TCS34725_RDATAL);
  *g = tcs.read16(TCS34725_GDATAL);
  *b = tcs.read16(TCS34725_BDATAL);
}


//---------------------------------------------------------
void setup()
{
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr, FALLING);
  Serial.begin(9600);
  LED_init();
  BC26_init();
  Serial.println("测试开始");
  if (tcs.begin()) 
  {
    Serial.println("已连接TCS34725传感器");
  } 
  else 
  {
    Serial.println("找不到TCS34725传感器");
    while (1);
  }
  while(1) 
  {
    if(!BC26_network_check()) continue;
    if(!BC26_Ali_connect()) continue;
    break;
  }
  tcs.write8(TCS34725_PERS, TCS34725_PERS_NONE); 
  tcs.setInterrupt(true);
  Serial.flush();
}

void loop()
{
  while (1)
  {
    get();
    if (lux < luxThreshold)
    {
      luxStatus = true;
      alarm_on();
      Serial.println("-> alarm !");
    }
    else
    {
      luxStatus = false;
      alarm_off();
      Serial.println("-> work fine!");
    }
    BC26_sensor_upload(lux, luxThreshold, luxStatus);
    BC26_sensor_download();
    Serial.println("--------------------------------------------------");
  }
  LED_init();
  while (1);
}

void get()
{
  getRawData_noDelay(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature(r, g, b);
  lux = tcs.calculateLux(r, g, b);
  Serial.print("Lux: "); Serial.print(lux, DEC); Serial.print(" - ");
  tcs.clearInterrupt();
  state = false;
}


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
    Serial.println("联网成功！");
    return flag;
  }
  else
  {
    Serial.println("联网失败！");
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

  cleanBuffer(ATcmd, BUF_LEN1);
  snprintf(ATcmd, BUF_LEN1, BC26_ALI_SUB_SET_t, ProductKey, DeviceName);
  flag = check_send_cmd(ATcmd, BC26_ALI_SUB_SET_t, DEFAULT_TIMEOUT);
  Serial.println("Subscribe thresholdChange");

//   cleanBuffer(ATcmd, BUF_LEN1);
//   snprintf(ATcmd, BUF_LEN1, AT_QMT_SUB_SET, ProductKey, DeviceName);
//   flag = check_send_cmd(ATcmd, AT_QMT_SUB_SET, DEFAULT_TIMEOUT);
//   Serial.println("Subscribe alarmFree");
  return flag;
}

//数据上传
bool BC26_sensor_upload(int lux, int luxThreshold, bool status)
{
  bool flag;
  cleanBuffer(ATcmd, BUF_LEN1);

  snprintf(ATcmd, BUF_LEN1, BC26_ALI_SENSOR_UPLOAD, ProductKey, DeviceName, lux, luxThreshold, status);
  flag = check_send_cmd(ATcmd, BC26_ALI_SENSOR_UPLOAD_SUCC, 20);
  return flag;
}

bool BC26_sensor_download()
{
  if (check_send_cmd("AT", "version", 1))
  {
    Serial.print("订阅的返回:");
    Serial.println(ATbuffer);
    int temp = ReadNum("luxThreshold");
    int temp1 = 20;
    Serial.println("temp:");
    Serial.println(temp);
    if (temp != 0)
    {
      luxThreshold = temp;
      temp1 = temp;
    }
    if (temp = 0)
    {
      luxThreshold = temp1;
    }
    Serial.println("Refreshed Local luxThreshold:");
    Serial.print(luxThreshold);
    Serial.println();
  }

  return false;
}

// bool BC26_sensor_download_alarm()
// {
//   if (check_send_cmd("check", ALARM_FREE, 1))
//   {
//     flag_alarmFree = true;
//   }
//   return false;
// }

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

# Arduino-GreenhouseMonitoring
## 基于Arduino UNO和阿里云IoT平台的智能温室监控系统
### Intelligent greenhouse monitoring platform based on Arduino UNO and Aliyun IoT

####  本系统基于Arduino UNO和阿里云IoT平台搭建，集成2个测量场景，4个节点，3种传感器。室内测量场景使用环境传感器BME280、空气质量传感器CCS811、RGB颜色识别传感器TCS34725三个节点，实现对温室内部温度、湿度、气压、环境光照强度以及CO2浓度的测量；室外测量场景使用环境传感器BME280单个节点对温室外部的温度、湿度、气压进行测量。室内与室外联合监控，并将数据汇总到一处，基本满足了对温室农业要求环境的测量与监控需求。

####  系统各节点以BC26模块通过NB-IoT连接方式与阿里云IoT平台进行通信，能够很好的适应覆盖较广地域的农业场景，可实现测量数据的实时上报与云平台数据的下发。用户可通过Web端查看各节点的测量数据，并修改各节点的报警阈值。各超过阈值的环境因素触发的警报可通过各节点的执行器（LED、蜂鸣器）、终端串口消息以及钉钉机器人推送实现。

### 🏷️ 传感器
#### * 环境传感器 BME280： 测量温度、湿度、气压
#### * 空气质量传感器 CCS811：测量CO2浓度
#### * RGB颜色识别传感器 TCS34725：测量环境光照强度


### 🏷️ 系统功能
#### * 包含2个测量场景，4个节点，3种传感器 
#### * 使用BC26模块通过NB-IoT方式与阿里云IoT平台通信
#### * 支持网页端查看实时数据与警报阈值修改与下发
#### * 支持钉钉机器人推送
#### * 支持单节点报警

### 🏷️ 系统框图
|<img src="https://user-images.githubusercontent.com/95619684/165228318-213f051c-78c4-4b1a-b036-d4020ba8dada.png" width="400"/> | <img src="https://user-images.githubusercontent.com/95619684/165228391-80c63b8b-7f82-4a1f-816f-fc58c37d8747.png" width="500"/> |
|:---:|:---:|

### 🏷️ Web端
<img src="https://user-images.githubusercontent.com/95619684/165229414-9f435e49-56a5-4303-9946-e7cfaefe949c.png" width="1000" align=“center”/>



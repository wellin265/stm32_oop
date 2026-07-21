#ifndef __ESP8266_MQTT_H_
#define __ESP8266_MQTT_H_

#include "stm32f1xx_hal.h"
#include "usart_ring.h"

/* WiFi工作模式 */
typedef enum {
    ESP8266_MODE_STA = 0,
    ESP8266_MODE_AP,
    ESP8266_MODE_STA_AP
} ESP8266_Mode_T;

/* MQTT配置结构体 */
typedef struct {
    USART_RING_S*  usart_ring;     /* 继承的环形串口对象 */
    GPIO_TypeDef*  rst_port;       /* RST引脚 */
    uint16_t       rst_pin;
    GPIO_TypeDef*  ch_pd_port;     /* CH_PD(使能)引脚 */
    uint16_t       ch_pd_pin;
    char*          ssid;           /* WiFi SSID */
    char*          password;       /* WiFi密码 */
    char*          mqtt_broker;    /* MQTT服务器地址 */
    char*          mqtt_client_id;
    char*          mqtt_username;
    char*          mqtt_password;
    char*          mqtt_pub_topic; /* 发布主题 */
    char*          mqtt_sub_topic; /* 订阅主题 */
} ESP8266_MQTT_CONFIG_T;

/* 前向声明 */
typedef struct ESP8266_MQTT_S ESP8266_MQTT_S;

/* 函数指针类型定义 */
typedef uint8_t (*ESP8266_MQTT_INIT_FUNC)(ESP8266_MQTT_S* obj);
typedef uint8_t (*ESP8266_MQTT_RECV_FUNC)(ESP8266_MQTT_S* obj);
typedef void (*ESP8266_MQTT_PUB_INT_FUNC)(ESP8266_MQTT_S* obj, char* name, int32_t val);
typedef void (*ESP8266_MQTT_PUB_FLOAT_FUNC)(ESP8266_MQTT_S* obj, char* name, float val, uint8_t precision);
typedef void (*ESP8266_MQTT_PUB_STR_FUNC)(ESP8266_MQTT_S* obj, char* name, char* val);

/* ESP8266 MQTT主结构体 */
struct ESP8266_MQTT_S {
    ESP8266_MQTT_CONFIG_T config;
    USART_RING_S* usart_ring;
    uint8_t mqtt_connected;        /* MQTT连接状态 */

    uint8_t (*init)(ESP8266_MQTT_S* obj);
    uint8_t (*recv)(ESP8266_MQTT_S* obj);
    void (*pub_int)(ESP8266_MQTT_S* obj, char* name, int32_t val);
    void (*pub_float)(ESP8266_MQTT_S* obj, char* name, float val, uint8_t precision);
    void (*pub_str)(ESP8266_MQTT_S* obj, char* name, char* val);
    /* 扩展数据字段(用户填充后可在recv中解析) */
    void* user_data;
};

/* 构造函数 */
void esp8266_mqtt_create(ESP8266_MQTT_S* obj, ESP8266_MQTT_CONFIG_T* config);

#endif

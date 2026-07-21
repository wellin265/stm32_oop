#include "esp8266_mqtt.h"
#include <stdio.h>
#include <string.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief  使能GPIO端口时钟
 * @param  port GPIO端口
 * @retval 无
 */
static void ESP8266_GPIO_ClkEnable(GPIO_TypeDef* port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
}

/**
 * @brief  控制ESP8266 RST引脚电平
 * @param  obj ESP8266对象
 * @param  level 0=低电平, 1=高电平
 * @retval 无
 */
static void esp8266_rst(ESP8266_MQTT_S* obj, uint8_t level)
{
    HAL_GPIO_WritePin(obj->config.rst_port, obj->config.rst_pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  控制ESP8266 CH_PD(使能)引脚电平
 * @param  obj ESP8266对象
 * @param  level 0=低电平, 1=高电平
 * @retval 无
 */
static void esp8266_ch_pd(ESP8266_MQTT_S* obj, uint8_t level)
{
    HAL_GPIO_WritePin(obj->config.ch_pd_port, obj->config.ch_pd_pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  发送AT指令并等待回复
 * @param  obj ESP8266对象
 * @param  cmd AT指令字符串
 * @param  reply 期待的回复内容(NULL表示不需要等待回复)
 * @param  timeout_ms 超时时间(毫秒)
 * @retval 1=成功收到回复, 0=超时或失败
 */
static uint8_t esp8266_at_cmd(ESP8266_MQTT_S* obj, char* cmd, char* reply, uint32_t timeout_ms)
{
    /* 发送AT指令 */
    obj->usart_ring->send(obj->usart_ring, "%s\r\n", cmd);

    /* 不需要回复则直接返回 */
    if (reply == NULL) return 1;

    /* 轮询串口接收缓存，等待回复 */
    char accum[256];
    uint16_t pos = 0;

    for (uint16_t i = 0; i < timeout_ms; i++) {
        uint8_t buf[64];
        uint16_t len = obj->usart_ring->read(obj->usart_ring, buf, sizeof(buf));
        if (len > 0) {
            for (uint16_t j = 0; j < len && pos < sizeof(accum) - 1; j++) {
                accum[pos++] = buf[j];
            }
            accum[pos] = '\0';

            if (strstr(accum, reply)) {
                return 1;
            }
        }
        HAL_Delay(1);
    }

    return 0;
}

/* ========== 接口实现 ========== */

/**
 * @brief  ESP8266 MQTT初始化(静态实现)
 *         流程: GPIO初始化 -> 使能模块 -> AT测试 -> 设置STA模式
 *               -> 连接WiFi -> MQTT参数配置 -> MQTT连接 -> MQTT订阅
 * @param  obj ESP8266对象
 * @retval 0=成功, 1=失败
 */
static uint8_t ESP8266_MQTT_Init(ESP8266_MQTT_S* obj)
{
    uint8_t retry;
    char cStr[300];
    GPIO_InitTypeDef gpio_init = {0};

    /* 1. 初始化RST和CH_PD引脚为推挽输出 */
    ESP8266_GPIO_ClkEnable(obj->config.rst_port);
    ESP8266_GPIO_ClkEnable(obj->config.ch_pd_port);

    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Pin = obj->config.rst_pin;
    HAL_GPIO_Init(obj->config.rst_port, &gpio_init);

    gpio_init.Pin = obj->config.ch_pd_pin;
    HAL_GPIO_Init(obj->config.ch_pd_port, &gpio_init);

    /* 2. 硬件复位ESP8266模块 */
    esp8266_ch_pd(obj, 1);    /* 使能模块 */
    esp8266_rst(obj, 0);      /* 拉低RST */
    HAL_Delay(500);           /* 保持复位 */
    esp8266_rst(obj, 1);      /* 释放RST */

    /* 等待ESP8266启动完成 */
    HAL_Delay(500);

    /* 清空启动时的乱码数据 */
    {
        uint8_t flush[64];
        obj->usart_ring->read(obj->usart_ring, flush, sizeof(flush));
    }

    /* 3. AT指令测试(最多重试10次) */
    retry = 0;
    while (retry < 10) {
        if (esp8266_at_cmd(obj, "AT", "OK", 500)) {
            break;
        }
        retry++;
        printf("ESP8266 AT retry %d...\r\n", retry);
    }
    if (retry >= 10) {
        printf("ESP8266 AT test failed\r\n");
        return 1;
    }
    printf("ESP8266 AT test OK\r\n");

    /* 4. 设置WiFi模式为STA */
    if (!esp8266_at_cmd(obj, "AT+CWMODE=1", "OK", 1000)) {
        printf("ESP8266 set STA mode failed\r\n");
        return 1;
    }

    /* 5. 使能DHCP */
    esp8266_at_cmd(obj, "AT+CWDHCP=1,1", "OK", 500);

    /* 6. 连接WiFi热点 */
    snprintf(cStr, sizeof(cStr), "AT+CWJAP=\"%s\",\"%s\"",
             obj->config.ssid ? obj->config.ssid : "",
             obj->config.password ? obj->config.password : "");
    if (!esp8266_at_cmd(obj, cStr, "OK", 8000)) {
        printf("ESP8266 WiFi join failed\r\n");
        return 1;
    }
    printf("ESP8266 WiFi connected\r\n");

    /* 7. 配置MQTT用户参数 */
    snprintf(cStr, sizeof(cStr), "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
             obj->config.mqtt_client_id ? obj->config.mqtt_client_id : "",
             obj->config.mqtt_username ? obj->config.mqtt_username : "",
             obj->config.mqtt_password ? obj->config.mqtt_password : "");
    if (!esp8266_at_cmd(obj, cStr, "OK", 1000)) {
        printf("ESP8266 MQTT user config failed\r\n");
        return 1;
    }

    /* 8. 连接MQTT服务器 */
    snprintf(cStr, sizeof(cStr), "AT+MQTTCONN=0,\"%s\",1883,0",
             obj->config.mqtt_broker ? obj->config.mqtt_broker : "");
    if (!esp8266_at_cmd(obj, cStr, "OK", 5000)) {
        printf("ESP8266 MQTT connect failed\r\n");
        return 1;
    }
    printf("ESP8266 MQTT connected\r\n");

    /* 9. 订阅MQTT主题 */
    snprintf(cStr, sizeof(cStr), "AT+MQTTSUB=0,\"%s\",0",
             obj->config.mqtt_sub_topic ? obj->config.mqtt_sub_topic : "");
    if (!esp8266_at_cmd(obj, cStr, "OK", 1000)) {
        printf("ESP8266 MQTT subscribe failed\r\n");
        return 1;
    }

    obj->mqtt_connected = 1;
    printf("ESP8266 MQTT init success\r\n");
    return 0;
}

/**
 * @brief  ESP8266 MQTT接收处理(静态实现)
 *         从串口环形缓冲读取数据，检查MQTT订阅消息并输出
 * @param  obj ESP8266对象
 * @retval 1=收到MQTT订阅数据, 0=无数据
 */
static uint8_t ESP8266_MQTT_Recv(ESP8266_MQTT_S* obj)
{
    uint8_t buf[256];
    uint16_t len = obj->usart_ring->read(obj->usart_ring, buf, sizeof(buf) - 1);

    if (len == 0) return 0;

    buf[len] = '\0';

    /* 检查是否包含MQTT订阅接收数据 */
    if (strstr((char*)buf, "+MQTTSUBRECV:")) {
        printf("MQTTSUB: %s\r\n", (char*)buf);
        return 1;
    }

    /* 输出其他ESP8266主动上报的消息 */
    printf("ESP8266: %s\r\n", (char*)buf);
    return 0;
}

/**
 * @brief  发布整型数据到MQTT(静态实现)
 *         格式: {"params":{"name":val},"version":"1.0.0"}
 * @param  obj ESP8266对象
 * @param  name 参数名称
 * @param  val 整型数值
 * @retval 无
 */
static void ESP8266_MQTT_PubInt(ESP8266_MQTT_S* obj, char* name, int32_t val)
{
    char cStr[400];

    snprintf(cStr, sizeof(cStr),
             "AT+MQTTPUB=0,\"%s\",\"{\\\"params\\\":{\\\"%s\\\":%ld}"
             "\\,\\\"version\\\":\\\"1.0.0\\\"}\",0,0",
             obj->config.mqtt_pub_topic ? obj->config.mqtt_pub_topic : "",
             name, (long)val);

    esp8266_at_cmd(obj, cStr, NULL, 1000);
}

/**
 * @brief  发布浮点数据到MQTT(静态实现)
 *         格式: {"params":{"name":val},"version":"1.0.0"}
 * @param  obj ESP8266对象
 * @param  name 参数名称
 * @param  val 浮点数值
 * @param  precision 小数位数
 * @retval 无
 */
static void ESP8266_MQTT_PubFloat(ESP8266_MQTT_S* obj, char* name, float val, uint8_t precision)
{
    char cStr[400];
    char valStr[32];

    /* 手动转换浮点数为字符串，避免依赖printf的%f支持 */
    {
        int32_t int_part = (int32_t)val;
        uint32_t frac_part = 0;

        if (precision > 0) {
            float frac = val - (float)int_part;
            if (frac < 0) frac = -frac;
            uint32_t mul = 1;
            for (uint8_t i = 0; i < precision; i++) mul *= 10;
            frac_part = (uint32_t)(frac * mul + 0.5f);
        }

        if (precision > 0) {
            snprintf(valStr, sizeof(valStr), "%ld.%0*u",
                     (long)int_part, precision, (unsigned int)frac_part);
        } else {
            snprintf(valStr, sizeof(valStr), "%ld", (long)int_part);
        }
    }

    snprintf(cStr, sizeof(cStr),
             "AT+MQTTPUB=0,\"%s\",\"{\\\"params\\\":{\\\"%s\\\":%s}"
             "\\,\\\"version\\\":\\\"1.0.0\\\"}\",0,0",
             obj->config.mqtt_pub_topic ? obj->config.mqtt_pub_topic : "",
             name, valStr);

    esp8266_at_cmd(obj, cStr, NULL, 1000);
}

/**
 * @brief  发布字符串数据到MQTT(静态实现)
 *         格式: {"params":{"name":"val"},"version":"1.0.0"}
 * @param  obj ESP8266对象
 * @param  name 参数名称
 * @param  val 字符串数值(不应包含引号等特殊字符)
 * @retval 无
 */
static void ESP8266_MQTT_PubStr(ESP8266_MQTT_S* obj, char* name, char* val)
{
    char cStr[400];

    snprintf(cStr, sizeof(cStr),
             "AT+MQTTPUB=0,\"%s\",\"{\\\"params\\\":{\\\"%s\\\":\\\"%s\\\"}"
             "\\,\\\"version\\\":\\\"1.0.0\\\"}\",0,0",
             obj->config.mqtt_pub_topic ? obj->config.mqtt_pub_topic : "",
             name, val);

    esp8266_at_cmd(obj, cStr, NULL, 1000);
}

/* ========== 构造函数 ========== */

/**
 * @brief  ESP8266 MQTT对象构造函数
 * @param  obj ESP8266对象指针
 * @param  config 配置参数指针
 * @retval 无
 */
void esp8266_mqtt_create(ESP8266_MQTT_S* obj, ESP8266_MQTT_CONFIG_T* config)
{
    if (obj == NULL || config == NULL || config->usart_ring == NULL) return;

    memset(obj, 0, sizeof(ESP8266_MQTT_S));
    obj->config = *config;
    obj->usart_ring = config->usart_ring;

    /* 绑定函数指针 */
    obj->init = ESP8266_MQTT_Init;
    obj->recv = ESP8266_MQTT_Recv;
    obj->pub_int = ESP8266_MQTT_PubInt;
    obj->pub_float = ESP8266_MQTT_PubFloat;
    obj->pub_str = ESP8266_MQTT_PubStr;

    /* 执行初始化 */
    obj->init(obj);
}

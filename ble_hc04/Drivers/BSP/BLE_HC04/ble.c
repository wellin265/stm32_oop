//
// Created by a1807 on 2026/5/20.
//
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "ble.h"
#include "ble_command.h"

static const uint32_t BLE_BAUD_RATES[] = {115200, 9600, 921600, 57600, 38400, 19200};
#define BLE_BAUD_RATES_COUNT (sizeof(BLE_BAUD_RATES) / sizeof(BLE_BAUD_RATES[0]))

static uint8_t BLE_TEST(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_AT_TEST);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint32_t BLE_GET_BAUD(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;
	uint32_t baud_rate = 0;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_GET_BAUD);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			char* baud_str = strstr((char*)buffer, "+BAUD:");
			if (baud_str != NULL)
			{
				// 跳过 "+BAUD=" 到数字部分
				baud_str += 6; // strlen("+BAUD=") = 6

				// 提取波特率数值
				char* endptr;
				baud_rate = strtoul(baud_str, &endptr, 10);

				// 成功提取到波特率
				if (endptr > baud_str)
				{
					return baud_rate;
				}
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return 0; // 获取波特率失败
			}
		}
		HAL_Delay(1);
	}
	return 0;
}

static uint8_t BLE_SET_BAUD(BLE_S* ble, uint32_t baud_rate, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_SET_BAUD, baud_rate);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_GET_VERSION(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_GET_VERSION);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			char* version_str = strstr((char*)buffer, "+VERSION:");
			if (version_str != NULL)
			{
				printf("BLE Version: %s\n", version_str + 9); // 跳过 "+VERSION:" 输出版本信息
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_SET_LED(BLE_S* ble, uint8_t on, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, on ? BLE_COMMAND_SET_LED_ON : BLE_COMMAND_SET_LED_OFF);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_GET_LED_STATUS(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_GET_LED_STATUS);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			char* status_str = strstr((char*)buffer, "+LED:");
			if (status_str != NULL)
			{
				return (strstr(status_str, "ON") != NULL) ? TRUE : FALSE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return 0; // 获取LED状态失败
			}
		}
		HAL_Delay(1);
	}
	return 0;
}

static uint8_t BLE_SET_DEFAULT(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_SET_DEFAULT);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_RESET(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_RESET);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_SET_BTMODE(BLE_S* ble, uint8_t on, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, on ? BLE_COMMAND_BTMODE_ON : BLE_COMMAND_BTMODE_OFF);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

static uint8_t BLE_GET_BTMODE(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;
	uint8_t mode = 0;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_GET_BTMODE);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			char* mode_str = strstr((char*)buffer, "+BTMODE=");
			if (mode_str != NULL)
			{
				// 跳过 "+BTMODE=" 到数字部分
				mode_str += 8; // strlen("+BTMODE=") = 8

				// 提取模式数值
				char* endptr;
				mode = (uint8_t)strtoul(mode_str, &endptr, 10);

				// 成功提取到模式值
				if (endptr > mode_str)
				{
					return mode;
				}
			}
		}
		HAL_Delay(1);
	}
	return 0;
}

static uint8_t BLE_SET_ROLE(BLE_S* ble, BLE_ROLE_MODE_T ble_role_mode, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;
	const char* expected_response = NULL;

	// 发送命令并设置期望的响应字符串
	switch (ble_role_mode)
	{
	case BLE_ROLE_SLAVE:
		{
			ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_SET_ROLE_SLAVE);
			expected_response = "Slave";
			break;
		}
	case BLE_ROLE_SPPMASTER:
		{
			ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_SET_ROLE_SPPMASTER);
			expected_response = "SppMaster";
			break;
		}
	case BLE_ROLE_BLEMASTER:
		{
			ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_SET_ROLE_BLEMASTER);
			expected_response = "BleMaster";
			break;
		}
	default:
		{
			return FALSE; // 无效的角色模式
		}
	}

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer) - 1);
		if (length > 0)
		{
			buffer[length] = '\0';

			// 检查是否返回期望的角色字符串
			if (strstr((char*)buffer, expected_response) != NULL)
			{
				return TRUE;
			}
			// 检查错误响应
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE; // 超时
}

static uint8_t BLE_GET_ROLE(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_GET_ROLE);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer) - 1);
		if (length > 0)
		{
			buffer[length] = '\0';

			if (strstr((char*)buffer, "Slave") != NULL)
			{
				return BLE_ROLE_SLAVE;
			}
			else if (strstr((char*)buffer, "SppMaster") != NULL)
			{
				return BLE_ROLE_SPPMASTER;
			}
			else if (strstr((char*)buffer, "BleMaster") != NULL)
			{
				return BLE_ROLE_BLEMASTER;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE; // 获取角色失败
			}
		}
		HAL_Delay(1);
	}
	return FALSE; // 超时
}

static uint8_t BLE_CLEAR(BLE_S* ble, uint16_t timeout_ms)
{
	uint8_t buffer[100];
	uint16_t length;

	ble->usart_ring->send(ble->usart_ring, BLE_COMMAND_CLEAR);

	for (uint16_t i = 0; i < timeout_ms; i++)
	{
		length = ble->usart_ring->read(ble->usart_ring, buffer, sizeof(buffer));
		if (length > 0)
		{
			buffer[length] = '\0';
			if (strstr((char*)buffer, "OK") != NULL)
			{
				return TRUE;
			}
			else if (strstr((char*)buffer, "ERROR") != NULL)
			{
				return FALSE;
			}
		}
		HAL_Delay(1);
	}
	return FALSE;
}

// 辅助函数：测试指定波特率下的BLE通信
static uint8_t ble_test_baud_rate(BLE_S* ble, uint32_t baud_rate, uint16_t timeout_ms)
{
    ble->usart_ring->set_baud_rate(ble->usart_ring, baud_rate);
    HAL_Delay(100);

    for (uint8_t retry = 0; retry < 3; retry++)
    {
        if (ble->test(ble, timeout_ms) == TRUE)
        {
            printf("BLE test OK at %lu baud (attempt %d)\n", baud_rate, retry + 1);
            return TRUE;
        }
    }

    return FALSE;
}

// 辅助函数：自动检测并设置最佳波特率
static uint32_t ble_auto_detect_baud_rate(BLE_S* ble, uint16_t timeout_ms)
{
    // 记录当前波特率以便恢复
    uint32_t original_baud = ble->usart_ring->cfg.baud_rate; // 默认值

    // 尝试所有预定义的波特率
    for (uint8_t i = 0; i < BLE_BAUD_RATES_COUNT; i++)
    {
        uint32_t test_baud = BLE_BAUD_RATES[i];
        printf("Trying baud rate: %lu\n", test_baud);

        if (ble_test_baud_rate(ble, test_baud, timeout_ms) == TRUE)
        {
            return test_baud; // 返回检测到的波特率
        }
    }
	ble->usart_ring->set_baud_rate(ble->usart_ring, original_baud);
    return 0; // 所有波特率都失败
}

// 辅助函数：将BLE模块切换到目标波特率
static uint8_t ble_switch_to_target_baud_rate(BLE_S* ble, uint32_t current_baud, uint32_t target_baud, uint16_t timeout_ms)
{
    if (current_baud == target_baud)
    {
        return TRUE; // 已经是目标波特率
    }

    // 确保使用当前波特率通信
    if (current_baud != 0)
    {
        ble->usart_ring->set_baud_rate(ble->usart_ring, current_baud);
        HAL_Delay(50);
    }

    // 发送设置波特率命令
    if (ble->set_baud(ble, target_baud, timeout_ms) == TRUE)
    {
        printf("Set BLE baud rate command sent, waiting for module to switch...\n");

        /* HC-04 模块在回复 "OK" 后，内部还需要时间完成时钟切换。
         * 经验值: 至少等待 300ms 后再切换 MCU 侧的 UART 波特率，
         * 否则模块侧还是旧波特率，MCU 侧已经是新波特率，通信失败。 */
        HAL_Delay(300);

        // 切换到新波特率
        ble->usart_ring->set_baud_rate(ble->usart_ring, target_baud);

        /* 模块侧切换完毕后，MCU 侧刚切过来，再给一段稳定时间。 */
        HAL_Delay(200);

        // 验证新波特率是否生效 (多次重试)
        for (uint8_t retry = 0; retry < 3; retry++)
        {
            if (ble->test(ble, timeout_ms / 2) == TRUE)
            {
                printf("BLE baud rate switch verified: %lu (attempt %d)\n",
                       target_baud, retry + 1);
                return TRUE;
            }
            printf("BLE verification retry %d...\n", retry + 1);
            HAL_Delay(100);
        }

        printf("BLE baud rate switch verification failed after retries\n");
        return FALSE;
    }

    return FALSE;
}

// 优化的ble_create函数
uint8_t ble_create(BLE_S* ble, USART_RING_S* usart_ring)
{
    uint8_t ret = FALSE;
    uint32_t detected_baud = 0;

    // 参数检查
    if (ble == NULL || usart_ring == NULL)
    {
        printf("BLE create failed: invalid parameters\n");
        return FALSE;
    }

    // 初始化BLE结构体
    memset(ble, 0, sizeof(BLE_S));
    ble->usart_ring = usart_ring;

    // 设置函数指针
    ble->test = BLE_TEST;
    ble->get_baud = BLE_GET_BAUD;
    ble->set_baud = BLE_SET_BAUD;
    ble->get_version = BLE_GET_VERSION;
    ble->set_led = BLE_SET_LED;
    ble->get_led_status = BLE_GET_LED_STATUS;
    ble->set_default = BLE_SET_DEFAULT;
    ble->reset = BLE_RESET;
    ble->set_btmode = BLE_SET_BTMODE;
    ble->get_btmode = BLE_GET_BTMODE;
    ble->set_role = BLE_SET_ROLE;
    ble->get_role = BLE_GET_ROLE;
    ble->clear = BLE_CLEAR;

    printf("Starting BLE module initialization...\n");

    // 1. 自动检测当前波特率
    detected_baud = ble_auto_detect_baud_rate(ble, 1000);

    if (detected_baud == 0)
    {
        printf("BLE module not responding at any baud rate!\n");
        printf("Please check:\n");
        printf("  - Physical connection\n");
        printf("  - Power supply\n");
        printf("  - Module status (LED indicator)\n");
        return FALSE;
    }

    printf("BLE module detected at %lu baud\n", detected_baud);

    // 2. 获取模块版本信息（可选，便于调试）
    printf("Getting version info...\n");
    ble->get_version(ble, 500);

    // 3. 检查当前波特率是否为期望值（115200）
    const uint32_t TARGET_BAUD = 115200;

    if (detected_baud != TARGET_BAUD)
    {
        printf("Switching baud rate from %lu to %lu...\n", detected_baud, TARGET_BAUD);

        if (ble_switch_to_target_baud_rate(ble, detected_baud, TARGET_BAUD, 2000) == TRUE)
        {
            ret = TRUE;
        }
        else
        {
            printf("Failed to switch to target baud rate, using current %lu\n", detected_baud);
            // 确保UART设置在检测到的波特率
            ble->usart_ring->set_baud_rate(ble->usart_ring, detected_baud);
            ret = TRUE; // 虽然没切换成功，但模块可用
        }
    }
    else
    {
        printf("BLE already at target baud rate %lu\n", TARGET_BAUD);
        ret = TRUE;
    }

    // 4. 最终验证
    if (ret == TRUE)
    {
        HAL_Delay(100);
        if (ble->test(ble, 500) == TRUE)
        {
            printf("BLE module initialization successful!\n");

            // 输出模块状态信息
            uint8_t led_status = ble->get_led_status(ble, 500);
            printf("LED status: %s\n", led_status ? "ON" : "OFF");

            uint8_t bt_mode = ble->get_btmode(ble, 500);
            printf("BT mode: %s\n", bt_mode ? "ON" : "OFF");

            uint8_t role = ble->get_role(ble, 500);
            const char* role_str = "Unknown";
            switch(role)
            {
                case BLE_ROLE_SLAVE: role_str = "Slave"; break;
                case BLE_ROLE_SPPMASTER: role_str = "SPP Master"; break;
                case BLE_ROLE_BLEMASTER: role_str = "BLE Master"; break;
            }
            printf("Role: %s\n", role_str);

            return TRUE;
        }
        else
        {
            printf("BLE final verification failed!\n");
            return FALSE;
        }
    }

    printf("BLE module initialization failed!\n");
    return FALSE;
}

# NORFLASH

## 这个工程能学什么
这是一个 SPI NOR Flash 存储芯片实验（支持 W25Qxx / BY25Qxx / NM25Qxx 系列），用面向对象的方式封装了完整的 Flash 驱动。
他演示了 SPI 通信、Flash 指令集（读 ID / 读写 / 扇区擦除 / 整片擦除）、自动扇区管理（写入前自动检查并擦除）、跨页写入拆分，以及阻塞和 DMA 两种传输方式。

## 运行后你会看到什么
- 先打印芯片 ID（如 "ID: 0xEF16" 表示 W25Q64）
- 用阻塞方式向地址 4096 写入 256 字节 0xAA，再读回并打印
- 用 DMA 方式向地址 8192 写入 256 字节 0xBB，再读回并打印

## 代码做了什么
主流程是：
1. 创建 NORFLASH 对象
2. 创建配置选项（绑定 SPI 句柄、CS 引脚）
3. 注册 NORFLASH 对象（内部自动初始化 GPIO、读取芯片 ID、W25Q256 自动启用 4 字节地址模式）
4. 阻塞方式写入 256 字节（自动处理扇区擦除）→ 读回 → 串口打印
5. DMA 方式写入 256 字节 → 读回 → 串口打印

## 建议重点看哪几段
- BSP/NORFLASH 下的驱动代码（SPI 命令交互模式：拉低 CS → 发命令 → 发地址 → 读写数据 → 拉高 CS）
- `NORFLASH_Write_Impl()` 中的扇区管理策略：读整扇区 → 检查目标区间是否已擦除 → 需要则擦除 → 合并用户数据 → 写回
- `norflash_write_nocheck()` 中的自动跨页拆分（256 字节/页）
- `NORFLASH_Read_DMA_Impl()` 中的零拷贝设计——命令/地址用阻塞，数据段直接用用户 buf 做 DMA 收发
- `norflash_wait_busy()` 中通过轮询状态寄存器判断 Flash 内部操作是否完成

## 你会学到的知识点
- SPI 通信协议（Mode 0/3、MSB first、软件 CS 控制）
- NOR Flash 指令集（Write Enable / Read Data / Page Program / Sector Erase / JEDEC ID 等）
- Flash 的硬件特性——写前必须擦除（只能将 1 写为 0），扇区擦除以 4KB 为单位
- 自动扇区管理：读-检查-擦除-合并-写回的完整流程
- 页写入的 256 字节边界自动拆分
- DMA 传输的零拷贝设计——命令段阻塞 + 数据段 DMA，用户 buf 直接作为 DMA 缓冲区
- 状态寄存器轮询（BUSY 位）等待 Flash 内部操作完成
- 多芯片兼容（W25Qxx / BY25Qxx / NM25Qxx）与自动 ID 识别
- 面向对象风格的存储设备驱动封装

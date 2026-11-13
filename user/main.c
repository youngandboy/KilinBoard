/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	main.c
	*
	*	说明： 		OneNET Mini 裸机工程入口
	*				当前只验证：启动 -> main() -> USART1 打印
	*				不包含 RTOS，后续可在本工程上继续移植
	*
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f10x.h"

//硬件驱动
#include "delay.h"
#include "usart.h"


/*
************************************************************
*	函数名称：	Hardware_Init
*
*	函数功能：	硬件初始化
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		初始化中断分组、SysTick 延时和 USART1
************************************************************
*/
static void Hardware_Init(void)
{

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);		//中断控制器分组设置

	Delay_Init();										//SysTick初始化，用于普通延时

	Usart1_Init(115200);								//USART1，PA9/PA10，115200 8N1

}

/*
************************************************************
*	函数名称：	main
*
*	函数功能：	裸机主循环：打印启动信息，然后每秒打印一次计数
*
*	入口参数：	无
*
*	返回参数：	0
*
*	说明：		串口助手设置为 115200、8 数据位、1 停止位、无校验
************************************************************
*/
int main(void)
{

	unsigned int counter = 0;

	Hardware_Init();									//硬件初始化

	UsartPrintf(USART1, "\r\n");
	UsartPrintf(USART1, "========================================\r\n");
	UsartPrintf(USART1, " OneNET Mini bare-metal project\r\n");
	UsartPrintf(USART1, " MCU  : %s, HSE %uMHz\r\n", BOARD_MCU_NAME, (unsigned int)(BOARD_HSE_HZ / 1000000U));
	UsartPrintf(USART1, " UART : USART1 PA9/PA10, 115200 8N1\r\n");
	UsartPrintf(USART1, " Build: %s %s\r\n", __DATE__, __TIME__);
	UsartPrintf(USART1, "========================================\r\n");
	UsartPrintf(USART1, "Hello from main()!\r\n\r\n");

	while(1)
	{

		UsartPrintf(USART1, "main loop is running, counter = %u\r\n", counter);

		counter++;

		DelayXms(1000);									//延时1秒

	}

}

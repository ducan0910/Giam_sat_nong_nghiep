#include "my_lora.h"
#include "spi.h"
#include "rtc.h"
#include <stdlib.h>

LoRa myLoRa;
uint32_t current_time_in_gateway;
volatile uint8_t LoRa_Rx_Flag = 0; // 0: chua co tin, 1: da nhan xong

uint8_t LoRa_receive_interrupt(LoRa* _LoRa, uint8_t* data, uint8_t length) 
{
  uint8_t irqFlags = LoRa_read(_LoRa, 0x12);
  uint8_t read_len = 0;

  if ((irqFlags & 0x40) != 0) 
	{
    uint8_t packet_size = LoRa_read(_LoRa, RegRxNbBytes); 
    uint8_t currentAddr = LoRa_read(_LoRa, RegFiFoRxCurrentAddr); 
    LoRa_write(_LoRa, 0x0D, currentAddr);      

    read_len = (length < packet_size) ? length : packet_size;

    for (int i = 0; i < read_len; i++) 
		{
      data[i] = LoRa_read(_LoRa, RegFiFo); 
    }
  }

  LoRa_write(_LoRa, RegIrqFlags, 0xFF); 

  return read_len;
}

void my_lora_init_default(void) 
{
  myLoRa = newLoRa(); 

  myLoRa.CS_port      = NSS_GPIO_Port;
  myLoRa.CS_pin       = NSS_Pin;
  myLoRa.reset_port   = RESET_GPIO_Port;
  myLoRa.reset_pin    = RESET_Pin;
  myLoRa.DIO0_port    = DIO0_GPIO_Port;
  myLoRa.DIO0_pin     = DIO0_Pin;
  myLoRa.hSPIx        = &hspi1;

  myLoRa.frequency             = 433;         
  myLoRa.spredingFactor        = SF_7;        
  myLoRa.bandWidth             = BW_125KHz;   
  myLoRa.crcRate               = CR_4_5;      
  myLoRa.power                 = POWER_14db;  
  myLoRa.overCurrentProtection = 100;         
  myLoRa.preamble              = 8;           

  if (LoRa_init(&myLoRa) != LORA_OK)  
	{ 
		while(1); 
  }

  LoRa_setSyncWord(&myLoRa, 0x12); 
	LoRa_gotoMode(&myLoRa, SLEEP_MODE);
}

uint8_t my_lora_transmit_wait_data(data_parameters* data, uint32_t timeout)
{
	uint8_t ackBuf[16];

	LoRa_transmit(&myLoRa, (uint8_t*)data, sizeof(data_parameters), 1000);
	
	LoRa_Rx_Flag = 0;
	LoRa_gotoMode(&myLoRa, RXCONTIN_MODE);
	
	uint32_t start_wait = HAL_GetTick();
	
	while(HAL_GetTick() - start_wait < timeout)
	{
		if(LoRa_Rx_Flag == 1)
		{
			LoRa_Rx_Flag = 0;
			uint8_t rx_len = LoRa_receive_interrupt(&myLoRa, ackBuf, 16);
			if(rx_len > 0 && ackBuf[0] == ID_GATEWAY && ackBuf[1] == data->ID)		//neu nhan duoc ack tu gateway va gui cho minh
			{
				current_time_in_gateway = (uint32_t)((ackBuf[2] << 8) | ackBuf[3]);		//cap nhat thoi gian hien tai cua gateway
				TEMP_THRESHOLD_HIGH = ackBuf[4];
				TEMP_THRESHOLD_LOW  = ackBuf[5];
				HUMIDITY_AIR_HIGH   = ackBuf[6];
				HUMIDITY_AIR_LOW    = ackBuf[7];
				HUMIDITY_SOIL_HIGH  = ackBuf[8];
				HUMIDITY_SOIL_LOW   = ackBuf[9];
				Time_Interval				= (uint16_t)((ackBuf[10] << 8) | ackBuf[11]);
				LoRa_gotoMode(&myLoRa, SLEEP_MODE);
				return 1;
			}
		}
	}
	LoRa_gotoMode(&myLoRa, SLEEP_MODE);
	return 0;
}

uint8_t my_lora_new_device(data_parameters* data, uint32_t timeout)
{
	uint8_t ackBuf[16];

	LoRa_transmit(&myLoRa, (uint8_t*)data, sizeof(data_parameters), 1000);
	
	LoRa_Rx_Flag = 0;
	LoRa_gotoMode(&myLoRa, RXCONTIN_MODE);
	
	uint32_t start_wait = HAL_GetTick();
	
	while(HAL_GetTick() - start_wait < timeout)
	{
		if(LoRa_Rx_Flag == 1)
		{
			LoRa_Rx_Flag = 0;
			uint8_t rx_len = LoRa_receive_interrupt(&myLoRa, ackBuf, 16);
			if(rx_len > 0 && ackBuf[0] == ID_GATEWAY && ackBuf[1] == data->ID)
			{
				data->ID = ackBuf[2];		//cap nhat ID moi 
				current_time_in_gateway = (uint32_t)((ackBuf[3] << 8) | ackBuf[4]);		//cap nhat thoi gian hien tai cua gateway
				TEMP_THRESHOLD_HIGH = ackBuf[5];
				TEMP_THRESHOLD_LOW  = ackBuf[6];
				HUMIDITY_AIR_HIGH   = ackBuf[7];
				HUMIDITY_AIR_LOW    = ackBuf[8];
				HUMIDITY_SOIL_HIGH  = ackBuf[9];
				HUMIDITY_SOIL_LOW   = ackBuf[10];
				Time_Interval				= (uint16_t)((ackBuf[11] << 8) | ackBuf[12]);
				LoRa_gotoMode(&myLoRa, SLEEP_MODE);
				return 1;
			}
		}
	}
	LoRa_gotoMode(&myLoRa, SLEEP_MODE);
	return 0;
}

/*tinh thoi gian 1 chu ki bang ID_node va thoi gian hien tai cua gateway*/
uint32_t calculate_sleep_seconds(uint8_t myID, uint16_t gateway_time)		
{
	uint32_t my_slot_start = (myID - 1) * SLOT_DURATION;
	uint32_t my_slot_end = my_slot_start + SLOT_DURATION;
	uint32_t sleep_duration = 0;
	if(gateway_time < my_slot_start)
	{
		sleep_duration = my_slot_start - gateway_time;
	}
	else
	{
		sleep_duration = Time_Interval - (gateway_time - my_slot_start);
	}
	return sleep_duration;
}

uint32_t get_current_rtc_time()
{
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
  uint32_t total_seconds = sTime.Hours * 3600 + sTime.Minutes * 60 + sTime.Seconds;
  return total_seconds;
}

uint8_t TDMA_Send_Emergency(data_parameters* data)
{
	uint32_t current_time = get_current_rtc_time();
	uint32_t sec_in_slot = current_time % SLOT_DURATION;
	//cho den vung free window
	if(sec_in_slot < TDMA_WINDOW)
	{
		HAL_Delay((TDMA_WINDOW-sec_in_slot)*1000);
	}
	//gui emergency voi retry 5 lan thoi gian ngau nhien
	for(int retry = 0; retry < 5; retry++)
	{
		data->cmd = CMD_EMERGENCY;
		//bat dau gui o thoi gian ngau nhien
		HAL_Delay(100 + rand()%200);
		if(my_lora_transmit_wait_data(data, 3000))
		{
			return 1;		//gui thanh cong
		}
		HAL_Delay(100 + rand()%200);
	}
	return 0;		//gui that bai
}

uint8_t TDMA_Join_network(data_parameters* data, uint32_t timeout)
{
	uint8_t ackBuf[16];
	uint32_t start_join = HAL_GetTick();
	uint8_t captured = 0;

	LoRa_gotoMode(&myLoRa, RXCONTIN_MODE);
	//node nghe trong thoi gian > 1 slot
	while(HAL_GetTick() - start_join < timeout)
	{
		if(LoRa_Rx_Flag == 1)
		{
			LoRa_Rx_Flag = 0;
			uint8_t rx_len = LoRa_receive_interrupt(&myLoRa, ackBuf, 16);
			if(rx_len > 0 && ackBuf[0] == ID_GATEWAY)	//neu bat duoc ack, se cho them 500ms nua
			{
				HAL_Delay(50);
			}
			else		//neu bat duoc data gui tu node, cho them 2s nua
			{
				HAL_Delay(100);
			}
			captured = 1;
			break;
		}
	}

	//thuc hien yeu cau join voi retry 3 lan
	for(int retry = 0; retry < 3; retry++)
	{
		data->cmd = CMD_JOIN;
		if(my_lora_new_device(data, 3000))
		{
			LoRa_gotoMode(&myLoRa, SLEEP_MODE);
			return 1;
		}
		HAL_Delay(300+ rand()%200);
	}
	return 0;
}

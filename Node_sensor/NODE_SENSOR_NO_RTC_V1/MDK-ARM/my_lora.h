#ifndef MY_LORA_H
#define MY_LORA_H

#include "main.h"
#include "my_sensor.h"
#include "LoRa.h"

#define FRAME_DURATION   42 // 20 phút = 1200 giây
#define SLOT_DURATION    2    // 6 giây m?i slot
#define TDMA_WINDOW      1    // 3 giây d?u d? g?i Data
#define FREE_WINDOW      1    // 3 giây sau d? g?i emergency

extern LoRa myLoRa;
extern uint32_t current_time_in_gateway;
extern volatile uint8_t LoRa_Rx_Flag;

uint8_t LoRa_receive_interrupt(LoRa* _LoRa, uint8_t* data, uint8_t length);
void my_lora_init_default(void);
uint8_t my_lora_transmit_wait_data(data_parameters* data, uint32_t timeout);
uint8_t my_lora_new_device(data_parameters* data, uint32_t timeout);

//TDMA functions
uint32_t calculate_sleep_seconds(uint8_t myID, uint16_t gateway_time);
uint8_t TDMA_Send_Emergency(data_parameters* data);
uint8_t TDMA_Join_network(data_parameters* data, uint32_t timeout);
#endif

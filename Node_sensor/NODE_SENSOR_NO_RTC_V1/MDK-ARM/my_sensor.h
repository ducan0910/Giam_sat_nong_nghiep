#ifndef MY_SENSOR_H
#define MY_SENSOR_H

#include "main.h"

typedef struct __attribute__((packed))
{
	uint8_t 	ID;
	uint8_t 	cmd;
	int16_t 	temp;
	uint16_t 	hum_air;
	uint16_t 	hum_soil;
	uint16_t 	battery;
	
} data_parameters;

typedef enum 
{
	CMD_DATA 			= 0x01,
	CMD_JOIN 			= 0x02,
	CMD_RTS				= 0x03,
	CMD_CTS				= 0x04,
	CMD_EMERGENCY = 0xFF
} CMD_t;

typedef enum
{
	ID_NEWDEVICE = 0x00,
	ID_GATEWAY	 = 0xFF
} ID_t;

extern uint8_t TEMP_THRESHOLD_HIGH;
extern uint8_t TEMP_THRESHOLD_LOW;
extern uint8_t HUMIDITY_AIR_HIGH;
extern uint8_t HUMIDITY_AIR_LOW;
extern uint8_t HUMIDITY_SOIL_HIGH;
extern uint8_t HUMIDITY_SOIL_LOW;
extern uint16_t Time_Interval;

int16_t read_temp(void);
uint16_t read_hum_air(void);
uint16_t read_hum_soil(void);
uint16_t read_battery(void);
void read_sensors(data_parameters* data);
void AHT20_Read(float* Temp, float* Humid);
uint8_t check_thresholds(data_parameters* data);

#endif

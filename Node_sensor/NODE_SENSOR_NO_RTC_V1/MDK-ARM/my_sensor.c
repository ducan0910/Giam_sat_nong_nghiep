#include "my_sensor.h"
#include <stdlib.h>
#include "i2c.h"

#define AHT_ADDR	0x38<<1
extern I2C_HandleTypeDef hi2c1;

int16_t read_temp(void)
{
	return 378;
}
uint16_t read_hum_air(void)
{
	return 801;
}
uint16_t read_hum_soil(void)
{
	return 309;
}
uint16_t read_battery(void)
{
	return 4;
}

void AHT20_Read(float* Temp, float* Humid)
{
	HAL_Delay(40);
	uint8_t dum[7];
	HAL_I2C_Mem_Read(&hi2c1, AHT_ADDR, 0x71, 1, dum, 1, 100);

	if(!(dum[0]&(1<<3)))
	{
		dum[0] = 0xBE, dum[1] = 0x08, dum[2] = 0x00;
		HAL_I2C_Master_Transmit(&hi2c1, AHT_ADDR, dum, 3, 100);
		HAL_Delay(10);
	}
	
	dum[0] = 0xAC, dum[1] = 0x33, dum[2] = 0x00;
	HAL_I2C_Master_Transmit(&hi2c1, AHT_ADDR, dum, 3, 100);
	HAL_Delay(80);
	
	do {
		HAL_I2C_Mem_Read(&hi2c1, AHT_ADDR, 0x71, 1, dum, 1, 100);
		HAL_Delay(1);
	} while(dum[0]&(1<<7));
	
	HAL_I2C_Master_Receive(&hi2c1, AHT_ADDR, dum, 7, 100);
	uint32_t h20 = (dum[1])<<16 | (dum[2])<<8 | (dum[3]);
	uint32_t t20 = (dum[3])<<16 | (dum[4])<<8 | dum[5];
	h20 = h20>>4;
	t20 = t20&0xFFFFF;
	*Temp = (t20 / 1048576.0)*200.0 - 50.0;
	*Humid = h20 / 10485.76;
}

void read_sensors(data_parameters* data)
{
	float aht20_temp = 0;
	float aht20_humair = 0;
	AHT20_Read(&aht20_temp, &aht20_humair);
	
	data->temp = (int16_t)(aht20_temp * 10);
	data->hum_air = (uint16_t)(aht20_humair * 10);
	data->hum_soil = read_hum_soil() + rand()%20;
	data->battery = read_battery();
}

uint8_t check_thresholds(data_parameters* data)
{
	if(data->temp > TEMP_THRESHOLD_HIGH * 10 			|| data->temp < TEMP_THRESHOLD_LOW * 10
			|| data->hum_air > HUMIDITY_AIR_HIGH * 10 		|| data->hum_air < HUMIDITY_AIR_LOW * 10
			|| data->hum_soil > HUMIDITY_SOIL_HIGH * 10 	|| data->hum_soil < HUMIDITY_SOIL_LOW * 10) return 1;
	else return 0;
}

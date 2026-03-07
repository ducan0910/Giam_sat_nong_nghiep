#include "my_power.h"
#include "rtc.h"

extern RTC_HandleTypeDef hrtc;
void SystemClock_Config(void);

void my_power_enter_stop_mode(uint32_t seconds)
{
	/*
	if (seconds == 0) return;

	RTC_TimeTypeDef sTime = {0, 0, 0};
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

  RTC_AlarmTypeDef sAlarm = {0};

  if (seconds > 86399) seconds = 86399; 

  sAlarm.AlarmTime.Hours   = seconds / 3600;
  sAlarm.AlarmTime.Minutes = (seconds % 3600) / 60;
  sAlarm.AlarmTime.Seconds = seconds % 60;
  sAlarm.Alarm = RTC_ALARM_A;

  HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);

  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
      return;
  }
	
	HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

  SystemClock_Config(); 
  HAL_ResumeTick();     

  HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
	*/
	HAL_Delay(seconds * 1000);
}

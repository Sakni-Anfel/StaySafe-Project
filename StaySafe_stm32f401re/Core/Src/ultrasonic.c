/*****************************************************************************************************************************
**********************************    Author  : Ehab Magdy Abdullah                      *************************************
**********************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *************************************
**********************************    Youtube : https://www.youtube.com/@EhabMagdyy      *************************************
******************************************************************************************************************************/


#include "ultrasonic.h"
#include "cmsis_os.h"




void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        if (edgeState == 0)
        {
            IC_Value1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            edgeState = 1;
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
        }
        else if (edgeState == 1)
        {
            IC_Value2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            edgeState = 2;

            if (IC_Value2 >= IC_Value1)
                pulseWidth_us = IC_Value2 - IC_Value1;
            else
                pulseWidth_us = (65535 - IC_Value1) + IC_Value2 + 1;

            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
        }
    }
}

/*
 * ADF4360 SPI PLL Driver
 *
 * Copyright 2015 Northwest Digital Radio
 *
 * Licensed under the GPL-2.
 *
 */
 
#ifndef ADF4360_H_
#define ADF4360_H_

#define ADF4360_CONTROL_REG							0
#define ADF4360_R_REG								1
#define ADF4360_N_REG								2

//  Register 0 (control) bit definitions
#define ADF4360_REG0_PRESCALER(x)					((x) << 22)
#define ADF4360_REG0_POWERDOWN(x)					(((x) & 0x03) << 20)
#define ADF4360_REG0_CHARGE_PUMP_CURR1_uA(x)		(((((x) - 31) / 31) & 0x7) << 18)
#define ADF4360_REG0_CHARGE_PUMP_CURR2_uA(x)		(((((x) - 31) / 31) & 0x7) << 15)
#define ADF4360_REG0_OUTPUT_PWR(x)					((x) << 12)  // XXX Can this be calculated like current?
#define ADF4360_REG0_MUTE_TIL_LOCK_EN				(1 << 11)
#define ADF4360_REG0_CP_GAIN						(1 << 10)
#define ADF4360_REG0_CP_THREESTATE_EN				(1 << 9)
#define ADF4360_REG0_PD_POLARITY_POS				(1 << 8)
#define ADF4360_REG0_MUXOUT(x)						((x) << 5)
#define ADF4360_REG0_COUNTER_RESET					(1 << 4)
#define ADF4360_REG0_CORE_POWER_mA(x)				(((((x) - 5) / 5) & 0x3) << 2)

#define ADF4360_MUXOUT_THREESTATE					0
#define ADF4360_MUXOUT_DIGITAL_LOCK_DETECT			1
#define ADF4360_MUXOUT_N_DIV_OUT					2
#define ADF4360_MUXOUT_R_DIV_OUT					3
#define ADF4360_MUXOUT_ANALOG_LOCK_DETECT			4
#define ADF4360_MUXOUT_SERIAL_DATA_OUT				5
#define ADF4360_MUXOUT_GND							6

//  Register 1 (R Latch) bit definitions
#define ADF4360_REG1_BAND_SELECT_CLOCK(x)			(((x) & 0x03) << 20)
#define ADF4360_REG1_TEST_MODE_EN					(1 << 19)
#define ADF4360_REG1_LOCK_PRECISION_5_CYCLES_EN		(1 << 18)
#define ADF4360_REG1_ANTI_BACKLASH(x)				(((x) & 0x03) << 16)
#define ADF4360_REG1_R_COUNTER(x)					(((x) & 0x3FFF) << 2)

//  Register 2 (N Latch) bit definitions
#define AD4360_REG2_PRESCALAR_INPUT					(1 << 23)
#define AD4360_REG2_DIVIDE_BY_2						(1 << 22)
#define AD4360_REG2_CP_GAIN							(1 << 21)
#define AD4360_REG2_B_COUNTER(x)					(((x) & 0x1FFF) << 8)
#define AD4360_REG2_A_COUNTER(x)					(((x) & 0x1F) << 2)

struct adf4360_platform_data {
	char	name[32];
};

#endif
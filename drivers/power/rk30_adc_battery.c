/* drivers/power/rk30_adc_battery.c
 *
 * battery detect driver for the rk30 
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/gpio.h>
#include <linux/adc.h>
#include <mach/iomux.h>
#include <mach/board.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/wakelock.h>

#if 0
#define DBG(x...)   printk(x)
#else
#define DBG(x...)
#endif

static int rk30_battery_dbg_level = 0;
module_param_named(dbg_level, rk30_battery_dbg_level, int, 0644);

#define pr_bat( args...) \
	do { \
		if (rk30_battery_dbg_level) { \
			pr_info(args); \
		} \
	} while (0)


/*******************Battery Driver Timihng Setup******************************/
#define	TIMER_MS_COUNTS				1000	// in ms
/* Cycle Counter Setup per Tasks */
#define	SLOPE_SECOND_COUNTS			  15
#define	DISCHARGE_MIN_SECOND		 100
#define	CHARGE_MIN_SECOND			 120
#define	CHARGE_MID_SECOND			 150
#define	CHARGE_MAX_SECOND			 250
#define CHARGE_FULL_DELAY_TIMES		  10
#define USBCHARGE_IDENTIFY_TIMES	   5

#define	NUM_VOLTAGE_SAMPLE				((SLOPE_SECOND_COUNTS * 1000) / TIMER_MS_COUNTS)	 
#define	NUM_DISCHARGE_MIN_SAMPLE		((DISCHARGE_MIN_SECOND * 1000) / TIMER_MS_COUNTS)	 
#define	NUM_CHARGE_MIN_SAMPLE			((CHARGE_MIN_SECOND * 1000) / TIMER_MS_COUNTS)	    
#define	NUM_CHARGE_MID_SAMPLE			((CHARGE_MID_SECOND * 1000) / TIMER_MS_COUNTS)	     
#define	NUM_CHARGE_MAX_SAMPLE			((CHARGE_MAX_SECOND * 1000) / TIMER_MS_COUNTS)	  
#define NUM_CHARGE_FULL_DELAY_TIMES		((CHARGE_FULL_DELAY_TIMES * 1000) / TIMER_MS_COUNTS)
#define NUM_USBCHARGE_IDENTIFY_TIMES	((USBCHARGE_IDENTIFY_TIMES * 1000) / TIMER_MS_COUNTS)

#define BAT_2V5_VALUE				2500

#if defined(CONFIG_ARCH_RK3066B)

#define  BAT_DEFINE_VALUE			1800
#elif defined(CONFIG_ARCH_RK2928)
#define  BAT_DEFINE_VALUE			3300
#else
#define  BAT_DEFINE_VALUE			2500
#endif

#define BATT_FILENAME "/data/bat_last_capacity.dat"

static struct wake_lock batt_wake_lock;


struct batt_vol_cal{
	u32 disp_cal;
	u32 dis_charge_vol;
	u32 charge_vol;
};

#ifdef CONFIG_BATTERY_RK30_VOL3V8
/** Single Cell Battery COnfiguration */

#define BATT_MAX_VOL_VALUE			4150	// Value for 100% charged. in mV
#define BATT_ZERO_VOL_VALUE			3380    // Value for discharge shutdown.
// #define BATT_NOMAL_VOL_VALUE		3800	// Nominal battery voltage. Not Used.

/* Resistor divider configuration */
#define BAT_PULL_UP_R				 200	// Resistor in Ohm
#if defined(CONFIG_ARCH_RK3066B)
#define BAT_PULL_DOWN_R				 100
#else
#define BAT_PULL_DOWN_R				 200
#endif

static struct batt_vol_cal  batt_table[] = {
		/* Genio 3.0.8+ kernel original table */
		{ 0, 3400, 3580},
		{ 1, 3420, 3600}, { 2, 3420, 3630}, { 3, 3475, 3660}, { 4, 3495, 3680}, { 5, 3505, 3690}, { 7, 3525, 3700},
		{ 9, 3540, 3710}, {11, 3557, 3720}, {13, 3570, 3730}, {15, 3580, 3750}, {17, 3610, 3775},
		{19, 3630, 3780}, {21, 3640, 3790}, {23, 3652, 3800}, {25, 3662, 3810}, {27, 3672, 3820},
		{29, 3680, 3830}, {31, 3687, 3840}, {33, 3693, 3850}, {35, 3699, 3860}, {37, 3705, 3870},
		{39, 3710, 3880}, {41, 3714, 3890}, {43, 3718, 3900}, {45, 3722, 3910}, {47, 3726, 3920},
		{49, 3730, 3930}, {51, 3734, 3940}, {53, 3738, 3950}, {55, 3742, 3955}, {57, 3746, 3960},
		{59, 3750, 3965}, {61, 3756, 3970}, {63, 3764, 3975}, {65, 3774, 3980}, {67, 3786, 3985},
		{69, 3800, 3990}, {71, 3808, 4010}, {73, 3817, 4020}, {75, 3827, 4030}, {77, 3845, 4040},
		{79, 3950, 4050}, {81, 3964, 4060}, {83, 3980, 4070}, {85, 3990, 4080}, {87, 4000, 4090},
		{89, 4010, 4100}, {91, 4020, 4110}, {93, 4030, 4120}, {95, 4040, 4130}, {97, 4050, 4140},
		{100, 4060, 4150},
/*
		{0,3500,3700},{10,3548,3814},{20,3593,3864},{30,3616,3903},{40,3643,3921},{50,3679,3945},
		{60,3731,3983},{70,3798,4027},{80,3872,4075},{90,3957,4131},{100,4064,4180},
*/
};

#else
/** Dual Cell Battery COnfiguration */

#define BATT_MAX_VOL_VALUE			8284	// Value for 100% charged. in mV
#define BATT_ZERO_VOL_VALUE			6800    // Value for discharge shutdown.
// #define BATT_NOMAL_VOL_VALUE		7600	// Nominal battery voltage. Not Used.

/* Resistor divider configuration */
#define BAT_PULL_UP_R				 300	// Resistor in Ohms
#define BAT_PULL_DOWN_R				 100

static struct batt_vol_cal  batt_table[] = {
	{ 0, 6800, 7400},
	{ 1, 6840, 7440}, { 2, 6880, 7480}, { 3, 6950, 7450}, { 5, 7010, 7510}, { 7, 7050, 7550},
	{ 9, 7080, 7580}, {11, 7104, 7604}, {13, 7140, 7640}, {15, 7160, 7660}, {17, 7220, 7720},
	{19, 7260, 7760}, {21, 7280, 7780}, {23, 7304, 7802}, {25, 7324, 7824}, {27, 7344, 7844},
	{29, 7360, 7860}, {31, 7374, 7874}, {33, 7386, 7886}, {35, 7398, 7898}, {37, 7410, 7910},
	{39, 7420, 7920}, {41, 7424, 7928}, {43, 7436, 7947}, {45, 7444, 7944}, {47, 7450, 7958},
	{49, 7460, 7965}, {51, 7468, 7975}, {53, 7476, 7990}, {55, 7482, 8000}, {57, 7492, 8005},
	{59, 7500, 8011}, {61, 7510, 8033}, {63, 7528, 8044}, {65, 7548, 8055}, {67, 7560, 8066},
	{69, 7600, 8070}, {71, 7618, 8075}, {73, 7634, 8080}, {75, 7654, 8085}, {77, 7690, 8100},
	{79, 7900, 8180}, {81, 7920, 8210}, {83, 7964, 8211}, {85, 8000, 8214}, {87, 8002, 8218},
	{89, 8012, 8220}, {91, 8022, 8235}, {93, 8110, 8260}, {95, 8140, 8290}, {97, 8170, 8300},
	{100,8200 ,8310},

};
#endif


#define BATT_NUM  ARRAY_SIZE(batt_table)

#define adc_to_voltage(adc_val)	((adc_val * BAT_DEFINE_VALUE * (BAT_PULL_UP_R + BAT_PULL_DOWN_R)) / (1024 * BAT_PULL_DOWN_R))

/********************************************************************************/

extern int dwc_vbus_status(void);
extern int get_msc_connect_flag(void);

struct rk30_adc_battery_data {
	int irq;
	
	//struct timer_list       timer;
	struct workqueue_struct *wq;
	struct delayed_work 	    delay_work;
	struct work_struct 	    dcwakeup_work;
	struct work_struct                   lowerpower_work;
	bool                    resume;
	
	struct rk30_adc_battery_platform_data *pdata;

	int                     full_times;
	
	struct adc_client       *client; 
	int                     adc_val;
	int                     adc_samples[NUM_VOLTAGE_SAMPLE+2];
	
	int                     bat_status;
	int                     bat_status_cnt;
	int                     bat_health;
	int                     bat_present;
	int                     bat_voltage;
	int                     bat_capacity;
	int                     bat_change;
	
	int                     old_charge_level;
	int                    *pSamples;
	int                     gBatCapacityDisChargeCnt;
	int                     gBatCapacityChargeCnt;
	int 	          		capacitytmp;
	int                     poweron_check;
	int                     suspend_capacity;

	int                     status_lock;

};
static struct rk30_adc_battery_data *gBatteryData;

enum {
	BATTERY_STATUS          = 0,
	BATTERY_HEALTH          = 1,
	BATTERY_PRESENT         = 2,
	BATTERY_CAPACITY        = 3,
	BATTERY_AC_ONLINE       = 4,
	BATTERY_STATUS_CHANGED	= 5,
	AC_STATUS_CHANGED   	= 6,
	BATTERY_INT_STATUS	    = 7,
	BATTERY_INT_ENABLE	    = 8,
};

typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;


#ifdef CONFIG_POWER_ON_CHARGER_DISPLAY
extern int battery_low;
extern int charger_in_logo;
#endif

static int rk30_adc_battery_load_capacity(void)
{
	char value[4];
	int* p = (int *)value;
	long fd = sys_open(BATT_FILENAME,O_RDONLY,0);

	if(fd < 0){
		pr_bat("rk30_adc_battery_load_capacity: open file /data/bat_last_capacity.dat failed\n");
		return -1;
	}

	sys_read(fd,(char __user *)value,4);
	sys_close(fd);

	return (*p);
}

static void rk30_adc_battery_put_capacity(int loadcapacity)
{
	char value[4];
	int* p = (int *)value;

#ifdef CONFIG_POWER_ON_CHARGER_DISPLAY
    if (battery_low == 1) {
		pr_bat("rk30_adc_battery_put_capacity: battery low not write capacity.\n");
		return;
    }
#endif

	long fd = sys_open( BATT_FILENAME, O_CREAT | O_RDWR, 0);

	if(fd < 0){
		pr_bat("rk30_adc_battery_put_capacity: open file /data/bat_last_capacity.dat failed\n");
		return;
	}
	
	*p = loadcapacity;
	sys_write(fd, (const char __user *)value, 4);

	sys_close(fd);
}

static void rk30_adc_battery_charge_enable(struct rk30_adc_battery_data *bat)
{
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;

	if (pdata->charge_set_pin != INVALID_GPIO){
		gpio_direction_output(pdata->charge_set_pin, pdata->charge_set_level);
	}
}

static void rk30_adc_battery_charge_disable(struct rk30_adc_battery_data *bat)
{
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;

	if (pdata->charge_set_pin != INVALID_GPIO){
		gpio_direction_output(pdata->charge_set_pin, 1 - pdata->charge_set_level);
	}
}

//extern int suspend_flag;
static int rk30_adc_battery_get_charge_level(struct rk30_adc_battery_data *bat)
{
	int charge_on = 0;
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;

#if defined (CONFIG_BATTERY_RK30_AC_CHARGE)
	if (pdata->dc_det_pin != INVALID_GPIO){
		if (gpio_get_value (pdata->dc_det_pin) == pdata->dc_det_level){
			charge_on = 1;
		}
	}
#endif

#if defined  (CONFIG_BATTERY_RK30_USB_CHARGE)
	if (charge_on == 0){
		if (suspend_flag)
			return;
		if (1 == dwc_vbus_status()) {

			if (0 == get_msc_connect_flag()) {
				if (++gBatUsbChargeCnt >= NUM_USBCHARGE_IDENTIFY_TIMES) {
					gBatUsbChargeCnt = NUM_USBCHARGE_IDENTIFY_TIMES + 1;
					charge_on = 1;
				}
			}
		}                   
		else {
			gBatUsbChargeCnt = 0;
			if (2 == dwc_vbus_status()) {
				charge_on = 1;
			}
		}
	}
#endif
	return charge_on;
}

//int old_charge_level;
static int rk30_adc_battery_status_samples(struct rk30_adc_battery_data *bat)
{
	int charge_level;
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;

	charge_level = rk30_adc_battery_get_charge_level(bat);

	if (charge_level != bat->old_charge_level){
		bat->old_charge_level = charge_level;
		bat->bat_change  = 1;
		
		if(charge_level) {            
			rk30_adc_battery_charge_enable(bat);
		}
		else{
			rk30_adc_battery_charge_disable(bat);
		}
		bat->bat_status_cnt = 0;
	}

	if(charge_level == 0)
	{   
		/** Discharge **/
		bat->full_times = 0;
		bat->bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	else
	{
		/** Charge **/
		if (pdata->charge_ok_pin == INVALID_GPIO) {
			/* No charge_ok_pin available */
			if (bat->bat_capacity == 100){
				if (bat->bat_status != POWER_SUPPLY_STATUS_FULL){
					bat->bat_status = POWER_SUPPLY_STATUS_FULL;
					bat->bat_change  = 1;
				}
			}
			else{
				bat->bat_status = POWER_SUPPLY_STATUS_CHARGING;
			}
		}
		else{  // pin of charge_ok_pin
			if (gpio_get_value(pdata->charge_ok_pin) != pdata->charge_ok_level){

				bat->full_times = 0;
				bat->bat_status = POWER_SUPPLY_STATUS_CHARGING;
			}
			else{
				bat->full_times++;

				if (bat->full_times >= NUM_CHARGE_FULL_DELAY_TIMES) {
					bat->full_times = NUM_CHARGE_FULL_DELAY_TIMES + 1;
				}

				if ((bat->full_times >= NUM_CHARGE_FULL_DELAY_TIMES) && (bat->bat_capacity >= 99)){
					if (bat->bat_status != POWER_SUPPLY_STATUS_FULL){
						bat->bat_status = POWER_SUPPLY_STATUS_FULL;
						bat->bat_capacity = 100;
						bat->bat_change  = 1;
					}
				}
				else{
					bat->bat_status = POWER_SUPPLY_STATUS_CHARGING;
				}
			}
		}
	}

	return charge_level;
}

#if 0 /* Astralix */
static int rk_adc_voltage(int value)
{
	int voltage;

	int ref_voltage; //reference_voltage
	int pullup_res;
	int pulldown_res;

	ref_voltage = gBatteryData ->pdata->reference_voltage;
	pullup_res = gBatteryData ->pdata->pull_up_res;
	pulldown_res = gBatteryData ->pdata->pull_down_res;

	if(ref_voltage && pullup_res && pulldown_res){
		
		voltage =  ((value * ref_voltage * (pullup_res + pulldown_res)) / (1024 * pulldown_res));
		
	}else{
		voltage = adc_to_voltage(value); 	
	}
		
		
	return voltage;

}
#endif

static int *pSamples;
static void rk30_adc_battery_voltage_samples(struct rk30_adc_battery_data *bat)
{
	int value;
	int i,*pStart = bat->adc_samples, num = 0;
	int level = rk30_adc_battery_get_charge_level(bat);


	value = bat->adc_val;
	adc_async_read(bat->client);

	*pSamples++ = adc_to_voltage(value);

	bat->bat_status_cnt++;
	if (bat->bat_status_cnt > NUM_VOLTAGE_SAMPLE)  bat->bat_status_cnt = NUM_VOLTAGE_SAMPLE + 1;

	num = pSamples - pStart;
	
	if (num >= NUM_VOLTAGE_SAMPLE){
		pSamples = pStart;
		num = NUM_VOLTAGE_SAMPLE;
		
	}

	value = 0;
	for (i = 0; i < num; i++){
		value += bat->adc_samples[i];
	}
	bat->bat_voltage = value / num;

	/* Check limits */
	if(1 == level){
		if(bat->bat_voltage >= batt_table[BATT_NUM-1].charge_vol + 10)
			bat->bat_voltage = batt_table[BATT_NUM-1].charge_vol + 10;
		else if(bat->bat_voltage <= batt_table[0].charge_vol - 10)
			bat->bat_voltage = batt_table[0].charge_vol - 10;
	}
	else{
		if(bat->bat_voltage >= batt_table[BATT_NUM-1].dis_charge_vol + 10)
			bat->bat_voltage = batt_table[BATT_NUM-1].dis_charge_vol + 10;
		else if(bat->bat_voltage <= batt_table[0].dis_charge_vol  - 10)
			bat->bat_voltage = batt_table[0].dis_charge_vol - 10;

	}

}
static int rk30_adc_battery_voltage_to_capacity(struct rk30_adc_battery_data *bat, int BatVoltage)
{
	int i = 0;
	int capacity = 0;

	struct batt_vol_cal *p;
	p = batt_table;

	if (rk30_adc_battery_get_charge_level(bat)) {
		/* calculate with charging mode values */
		if(BatVoltage >= (p[BATT_NUM - 1].charge_vol)) {
			capacity = 100;
		}	
		else{
			if(BatVoltage <= (p[0].charge_vol)){
				capacity = 0;
			}
			else{
				for(i = 0; i < BATT_NUM - 1; i++){

					if(((p[i].charge_vol) <= BatVoltage) && (BatVoltage < (p[i+1].charge_vol))){
						capacity = p[i].disp_cal + ((BatVoltage - p[i].charge_vol) *  (p[i+1].disp_cal -p[i].disp_cal ))/ (p[i+1].charge_vol- p[i].charge_vol);
						break;
					}
				}
			}  
		}

	}
	else {
		/* Calculate with lower discharging values */
		if(BatVoltage >= (p[BATT_NUM - 1].dis_charge_vol)){
			capacity = 100;
		}	
		else{
			if(BatVoltage <= (p[0].dis_charge_vol)){
				capacity = 0;
			}
			else{
				for(i = 0; i < BATT_NUM - 1; i++){
					if(((p[i].dis_charge_vol) <= BatVoltage) && (BatVoltage < (p[i+1].dis_charge_vol))){
						capacity =  p[i].disp_cal+ ((BatVoltage - p[i].dis_charge_vol) * (p[i+1].disp_cal -p[i].disp_cal ) )/ (p[i+1].dis_charge_vol- p[i].dis_charge_vol) ;
						break;
					}
				}
			}  

		}


	}
    return capacity;
}

#ifdef CONFIG_BACKLIGHT_RK29_BL
extern void bl_check_low_battery(void);
static int bl_check_battery = 0, bl_check_flag = 0;
#endif

static void rk30_adc_battery_capacity_samples(struct rk30_adc_battery_data *bat)
{
	int capacity = 0;
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;

	//��ŵ�״̬�仯��Buffer����֮ǰ��������
	if (bat->bat_status_cnt < NUM_VOLTAGE_SAMPLE)  {
		bat->gBatCapacityDisChargeCnt = 0;
		bat->gBatCapacityChargeCnt    = 0;
		return;
	}
	
	capacity = rk30_adc_battery_voltage_to_capacity(bat, bat->bat_voltage);
	    
	if (rk30_adc_battery_get_charge_level(bat)){
		if (capacity > bat->bat_capacity){
			//ʵ�ʲ������������ʾ��������������
			if (++(bat->gBatCapacityDisChargeCnt) >= NUM_CHARGE_MIN_SAMPLE){
				bat->gBatCapacityDisChargeCnt  = 0;
				if (bat->bat_capacity < 99){
					bat->bat_capacity++;
					bat->bat_change  = 1;
				}
			}
			bat->gBatCapacityChargeCnt = 0;
		}
		else{  //   ʵ�ʵ������Ȳ���� ��ʾ������С
		            bat->gBatCapacityDisChargeCnt = 0;
		            (bat->gBatCapacityChargeCnt)++;
            
			if (pdata->charge_ok_pin != INVALID_GPIO){
				if (gpio_get_value(pdata->charge_ok_pin) == pdata->charge_ok_level){
				//��⵽��س����־��ͬʱ��ʱ���ڳ���ѹ�ޱ仯����ʼ������ʱ��磬������������
					if (bat->gBatCapacityChargeCnt >= NUM_CHARGE_MIN_SAMPLE){
						bat->gBatCapacityChargeCnt = 0;
						if (bat->bat_capacity < 99){
							bat->bat_capacity++;
							bat->bat_change  = 1;
						}
					}
				}
				else{
#if 0					
					if (capacity > capacitytmp){
					//���������ѹ����������ʱ����λ����ֹ��ʱ��ģ�����ʵ�ʳ���
						gBatCapacityChargeCnt = 0;
					}
					else if (/*bat->bat_capacity >= 85) &&*/ (gBatCapacityChargeCnt > NUM_CHARGE_MAX_SAMPLE)){
						gBatCapacityChargeCnt = (NUM_CHARGE_MAX_SAMPLE - NUM_CHARGE_MID_SAMPLE);

						if (bat->bat_capacity < 99){
						bat->bat_capacity++;
						bat->bat_change  = 1;
						}
					}
				}
#else			//  ��ֹ����ϻ�����ֳ岻��������
					if (capacity > bat->capacitytmp){
					//���������ѹ����������ʱ����λ����ֹ��ʱ��ģ�����ʵ�ʳ���
						bat->gBatCapacityChargeCnt = 0;
					}
					else{

						if ((bat->bat_capacity >= 85) &&((bat->gBatCapacityChargeCnt) > NUM_CHARGE_MAX_SAMPLE)){
							bat->gBatCapacityChargeCnt = (NUM_CHARGE_MAX_SAMPLE - NUM_CHARGE_MID_SAMPLE);

							if (bat->bat_capacity < 99){
								bat->bat_capacity++;
								bat->bat_change  = 1;
							}
						}
					}
				}
#endif

			}
			else{
			//û�г������ţ���ʱ���ڵ�ѹ�ޱ仯����ʱ��ģ����
				if (capacity > bat->capacitytmp){
				//���������ѹ����������ʱ����λ����ֹ��ʱ��ģ�����ʵ�ʳ���
					bat->gBatCapacityChargeCnt = 0;
				}
				else{

					if ((bat->bat_capacity >= 85) &&(bat->gBatCapacityChargeCnt > NUM_CHARGE_MAX_SAMPLE)){
						bat->gBatCapacityChargeCnt = (NUM_CHARGE_MAX_SAMPLE - NUM_CHARGE_MID_SAMPLE);

						if (bat->bat_capacity < 99){
							bat->bat_capacity++;
							bat->bat_change  = 1;
						}
					}
				}
				

			}            
		}
	}    
	else{   
	//�ŵ�ʱ,ֻ�����ѹ�½�
		if (capacity < bat->bat_capacity) 
		{
			if (++(bat->gBatCapacityDisChargeCnt) >= NUM_DISCHARGE_MIN_SAMPLE) {
				bat->gBatCapacityDisChargeCnt = 0;
				if (bat->bat_capacity > 0){
					bat->bat_capacity-- ;
					bat->bat_change  = 1;
				}
			}
		}
		else{
			bat->gBatCapacityDisChargeCnt = 0;
		}
		bat->gBatCapacityChargeCnt = 0;
	}
		bat->capacitytmp = capacity;

#ifdef CONFIG_BACKLIGHT_RK29_BL
    if (bat->bat_capacity < 10) {
        if (bl_check_battery < 0)
            bl_check_battery = 0;
        bl_check_battery++;

        if (bl_check_battery > 30) {
            if (bl_check_flag != 1) {
                bl_check_flag = 1;
                bl_check_low_battery();
            }

            bl_check_battery = 0;
        }
    } else {
        if (bl_check_battery > 0)
            bl_check_battery = 0;
        bl_check_battery--;

        if (bl_check_battery < -30) {
            if (bl_check_flag != 0) {
                bl_check_flag = 0;
                bl_check_low_battery();
            }

            bl_check_battery = 0;
        }
    }
#endif
}

int bl_check_battery_capacity(void)
{
    if ((gBatteryData->bat_capacity < 10) && (bl_check_flag == 1))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(bl_check_battery_capacity);

//static int poweron_check = 0;
static void rk30_adc_battery_poweron_capacity_check(void)
{
	int i;
	int level,oldlevel;
	struct rk30_adc_battery_data *bat = gBatteryData;
	int new_capacity, old_capacity;

	adc_sync_read(bat->client);                             //start adc sample
	level = oldlevel = rk30_adc_battery_status_samples(bat);//init charge status

	bat->full_times = 0;
	for (i = 0; i < NUM_VOLTAGE_SAMPLE; i++){                //0.3 s
		mdelay(1);
		rk30_adc_battery_voltage_samples(bat);              //get voltage
		//level = rk30_adc_battery_status_samples(bat);       //check charge status
		level = rk30_adc_battery_get_charge_level(bat);

		if (oldlevel != level){
			oldlevel = level;                               //if charge status changed, reset sample
			i = 0;
		}        
	}

	bat->bat_capacity = rk30_adc_battery_voltage_to_capacity(bat, bat->bat_voltage);  //init bat_capacity

	new_capacity = gBatteryData->bat_capacity;
	old_capacity = rk30_adc_battery_load_capacity();
	if (new_capacity == 0) {
	    new_capacity = 1;
	}
	if ((old_capacity <= 0) || (old_capacity >= 100)){
		old_capacity = new_capacity;
	}    
	if (old_capacity == 0) {
	    old_capacity = 1;
	}

#ifdef CONFIG_POWER_ON_CHARGER_DISPLAY
    if (charger_in_logo == 1) {
        gBatteryData->bat_capacity = old_capacity;
        return;
    }
#endif

	if (gBatteryData->bat_status == POWER_SUPPLY_STATUS_FULL){
		if (new_capacity > 80){
			gBatteryData->bat_capacity = 100;
		}
	}
	else if (gBatteryData->bat_status != POWER_SUPPLY_STATUS_NOT_CHARGING){
	//charging state
	//���⣺
//	//1����ʱ��ػ���ú󣬿�����ȡ������ԶԶ����ʵ��������ô�죿
//	//2���������������ʱ��ػ��ٿ���ǰ��������һ���ָ���ô�죿
//	//3��һ�����ַ�ʽ���ʣ�
	//gBatteryData->bat_capacity = new_capacity;
		gBatteryData->bat_capacity = (new_capacity > (old_capacity + 10)) ? new_capacity : old_capacity;
	}else{

		if(new_capacity > old_capacity + 50 )
			gBatteryData->bat_capacity = new_capacity;
		else
			gBatteryData->bat_capacity = ((new_capacity + 10) < old_capacity) ? new_capacity : old_capacity;  //avoid the value of capacity increase 
	}


	//printk("capacity = %d, new_capacity = %d, old_capacity = %d\n",gBatteryData->bat_capacity, new_capacity, old_capacity);

	gBatteryData->bat_change = 1;
}

#if defined(CONFIG_BATTERY_RK30_USB_CHARGE)
static int rk30_adc_battery_get_usb_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	charger_type_t charger;
	charger =  CHARGER_USB;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = get_msc_connect_flag();
		printk("%s:%d\n",__FUNCTION__,val->intval);
		break;

	default:
		return -EINVAL;
	}
	
	return 0;

}

static enum power_supply_property rk30_adc_battery_usb_props[] = {
    
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply rk30_usb_supply = 
{
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,

	.get_property   = rk30_adc_battery_get_usb_property,

	.properties     = rk30_adc_battery_usb_props,
	.num_properties = ARRAY_SIZE(rk30_adc_battery_usb_props),
};
#endif

#if defined(CONFIG_BATTERY_RK30_AC_CHARGE)
static irqreturn_t rk30_adc_battery_dc_wakeup(int irq, void *dev_id)
{   
	queue_work(gBatteryData->wq, &gBatteryData->dcwakeup_work);
	return IRQ_HANDLED;
}


static int rk30_adc_battery_get_ac_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;
	charger_type_t charger;
	charger =  CHARGER_USB;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
		{
			if (rk30_adc_battery_get_charge_level(gBatteryData))
			{
				val->intval = 1;
				}
			else
				{
				val->intval = 0;	
				}
		}
		DBG("%s:%d\n",__FUNCTION__,val->intval);
		break;
		
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property rk30_adc_battery_ac_props[] = 
{
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply rk30_ac_supply = 
{
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,

	.get_property   = rk30_adc_battery_get_ac_property,

	.properties     = rk30_adc_battery_ac_props,
	.num_properties = ARRAY_SIZE(rk30_adc_battery_ac_props),
};

static void rk30_adc_battery_dcdet_delaywork(struct work_struct *work)
{
	int ret;
	struct rk30_adc_battery_platform_data *pdata;
	int irq;
	int irq_flag;
	
	pdata    = gBatteryData->pdata;
	irq        = gpio_to_irq(pdata->dc_det_pin);
	irq_flag = gpio_get_value (pdata->dc_det_pin) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;

	rk28_send_wakeup_key(); // wake up the system

	free_irq(irq, NULL);
	ret = request_irq(irq, rk30_adc_battery_dc_wakeup, irq_flag, "ac_charge_irq", NULL);// reinitialize the DC irq 
	if (ret) {
		free_irq(irq, NULL);
	}

	power_supply_changed(&rk30_ac_supply);

	gBatteryData->bat_status_cnt = 0;        //the state of battery is change

}


#endif

static int rk30_adc_battery_get_status(struct rk30_adc_battery_data *bat)
{
	return (bat->bat_status);
}

static int rk30_adc_battery_get_health(struct rk30_adc_battery_data *bat)
{
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int rk30_adc_battery_get_present(struct rk30_adc_battery_data *bat)
{
	return (bat->bat_voltage < BATT_MAX_VOL_VALUE) ? 0 : 1;
}

static int rk30_adc_battery_get_voltage(struct rk30_adc_battery_data *bat)
{
	return (bat->bat_voltage );
}

static int rk30_adc_battery_get_capacity(struct rk30_adc_battery_data *bat)
{
	return (bat->bat_capacity);
}

static int rk30_adc_battery_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{		
	int ret = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = rk30_adc_battery_get_status(gBatteryData);
			DBG("gBatStatus=%d\n",val->intval);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = rk30_adc_battery_get_health(gBatteryData);
			DBG("gBatHealth=%d\n",val->intval);
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = rk30_adc_battery_get_present(gBatteryData);
			DBG("gBatPresent=%d\n",val->intval);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val ->intval = rk30_adc_battery_get_voltage(gBatteryData);
			DBG("gBatVoltage=%d\n",val->intval);
			break;
		//	case POWER_SUPPLY_PROP_CURRENT_NOW:
		//		val->intval = 1100;
		//		break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = rk30_adc_battery_get_capacity(gBatteryData);
			DBG("gBatCapacity=%d%%\n",val->intval);
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;	
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
			val->intval = BATT_MAX_VOL_VALUE;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
			val->intval = BATT_ZERO_VOL_VALUE;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static enum power_supply_property rk30_adc_battery_props[] = {

	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
//	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
};

static struct power_supply rk30_battery_supply = 
{
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,

	.get_property   = rk30_adc_battery_get_property,

	.properties     = rk30_adc_battery_props,
	.num_properties = ARRAY_SIZE(rk30_adc_battery_props),
};

#ifdef CONFIG_PM
static void rk30_adc_battery_resume_check(void)
{
	int i;
	int level,oldlevel;
	int new_capacity, old_capacity;
	struct rk30_adc_battery_data *bat = gBatteryData;

	bat->old_charge_level = -1;
	pSamples = bat->adc_samples;

	adc_sync_read(bat->client);                             //start adc sample
	level = oldlevel = rk30_adc_battery_status_samples(bat);//init charge status

	for (i = 0; i < NUM_VOLTAGE_SAMPLE; i++) {               //0.3 s   
	
		mdelay(1);
		rk30_adc_battery_voltage_samples(bat);              //get voltage
		level = rk30_adc_battery_status_samples(bat);       //check charge status
		if (oldlevel != level){		
		    oldlevel = level;                               //if charge status changed, reset sample
		    i = 0;
		}        
	}
	new_capacity = rk30_adc_battery_voltage_to_capacity(bat, bat->bat_voltage);
	old_capacity =gBatteryData-> suspend_capacity;

	if (bat->bat_status != POWER_SUPPLY_STATUS_NOT_CHARGING) {
		/* charging state */
		bat->bat_capacity = (new_capacity > (old_capacity+10)) ? new_capacity : old_capacity;
	}
	else{
		bat->bat_capacity = ((new_capacity + 10) < old_capacity) ? new_capacity : old_capacity;  // aviod the value of capacity increase dicharge
	}

}

static int rk30_adc_battery_suspend(struct platform_device *dev, pm_message_t state)
{
	int irq;
	gBatteryData->suspend_capacity = gBatteryData->bat_capacity;
	cancel_delayed_work(&gBatteryData->delay_work);
	
	if( gBatteryData->pdata->batt_low_pin != INVALID_GPIO){
		
		irq = gpio_to_irq(gBatteryData->pdata->batt_low_pin);
		enable_irq(irq);
	    	enable_irq_wake(irq);
    	}

	return 0;
}

static int rk30_adc_battery_resume(struct platform_device *dev)
{
	int irq;
	gBatteryData->resume = true;
	queue_delayed_work(gBatteryData->wq, &gBatteryData->delay_work, msecs_to_jiffies(100));
	if( gBatteryData->pdata->batt_low_pin != INVALID_GPIO){
		
		irq = gpio_to_irq(gBatteryData->pdata->batt_low_pin);
	    	disable_irq_wake(irq);
		disable_irq(irq);
    	}
	return 0;
}
#else
#define rk30_adc_battery_suspend NULL
#define rk30_adc_battery_resume NULL
#endif


unsigned long AdcTestCnt = 0;
static void rk30_adc_battery_timer_work(struct work_struct *work)
{
#ifdef CONFIG_PM
	if (gBatteryData->resume) {
		rk30_adc_battery_resume_check();
		gBatteryData->resume = false;
	}
#endif

	rk30_adc_battery_status_samples(gBatteryData);

	if (gBatteryData->poweron_check){   
		gBatteryData->poweron_check = 0;
		rk30_adc_battery_poweron_capacity_check();
	}

	rk30_adc_battery_voltage_samples(gBatteryData);
	rk30_adc_battery_capacity_samples(gBatteryData);

	if( 1 == rk30_adc_battery_get_charge_level(gBatteryData))   // charge
	{
		if(0 == gBatteryData->status_lock ) {			
			wake_lock(&batt_wake_lock);  //lock
			gBatteryData->status_lock = 1; 
		}
	}
	else{
		if(1 == gBatteryData->status_lock ) {			
			wake_unlock(&batt_wake_lock);  //unlock
			gBatteryData->status_lock = 0; 
		}
	}
	
	
	/*update battery parameter after adc and capacity has been changed*/
	if(gBatteryData->bat_change){
		gBatteryData->bat_change = 0;
		rk30_adc_battery_put_capacity(gBatteryData->bat_capacity);
		power_supply_changed(&rk30_battery_supply);
#if  defined (CONFIG_BATTERY_RK30_AC_CHARGE)
		if (gBatteryData->pdata->dc_det_pin == INVALID_GPIO){
			power_supply_changed(&rk30_ac_supply);
		}
#endif
	}

	if (rk30_battery_dbg_level)
	{
		if (++AdcTestCnt >= 2)
			{
			AdcTestCnt = 0;

			printk("Status = %d, RealAdcVal = %d, RealVol = %d,gBatVol = %d, gBatCap = %d, RealCapacity = %d, dischargecnt = %d, chargecnt = %d\n", 
			gBatteryData->bat_status, gBatteryData->adc_val, adc_to_voltage(gBatteryData->adc_val),
			gBatteryData->bat_voltage, gBatteryData->bat_capacity, gBatteryData->capacitytmp, gBatteryData->gBatCapacityDisChargeCnt,gBatteryData-> gBatCapacityChargeCnt);

		}
	}
	queue_delayed_work(gBatteryData->wq, &gBatteryData->delay_work, msecs_to_jiffies(TIMER_MS_COUNTS));

}


static int rk30_adc_battery_io_init(struct rk30_adc_battery_platform_data *pdata)
{
	int ret = 0;
	
	if (pdata->io_init) {
		pdata->io_init();
	}
	
	//charge control pin
	if (pdata->charge_set_pin != INVALID_GPIO){
	    	ret = gpio_request(pdata->charge_set_pin, NULL);
	    	if (ret) {
	    		printk("failed to request dc_det gpio\n");
	    		goto error;
		    	}
	    	gpio_direction_output(pdata->charge_set_pin, 1 - pdata->charge_set_level);
	}
	
	//dc charge detect pin
	if (pdata->dc_det_pin != INVALID_GPIO){
	    	ret = gpio_request(pdata->dc_det_pin, NULL);
	    	if (ret) {
	    		printk("failed to request dc_det gpio\n");
	    		goto error;
	    	}
	
	    	gpio_pull_updown(pdata->dc_det_pin, GPIOPullUp);//important
	    	ret = gpio_direction_input(pdata->dc_det_pin);
	    	if (ret) {
	    		printk("failed to set gpio dc_det input\n");
	    		goto error;
	    	}
	}
	
	//charge ok detect
	if (pdata->charge_ok_pin != INVALID_GPIO){
 		ret = gpio_request(pdata->charge_ok_pin, NULL);
	    	if (ret) {
	    		printk("failed to request charge_ok gpio\n");
	    		goto error;
	    	}
	
	    	gpio_pull_updown(pdata->charge_ok_pin, GPIOPullUp);//important
	    	ret = gpio_direction_input(pdata->charge_ok_pin);
	    	if (ret) {
	    		printk("failed to set gpio charge_ok input\n");
	    		goto error;
	    	}
	}
	//batt low pin
	if( pdata->batt_low_pin != INVALID_GPIO){
 		ret = gpio_request(pdata->batt_low_pin, NULL);
	    	if (ret) {
	    		printk("failed to request batt_low_pin gpio\n");
	    		goto error;
	    	}
	
	    	gpio_pull_updown(pdata->batt_low_pin, GPIOPullUp); 
	    	ret = gpio_direction_input(pdata->batt_low_pin);
	    	if (ret) {
	    		printk("failed to set gpio batt_low_pin input\n");
	    		goto error;
	    	}
	}
    
	return 0;
error:
	return -1;
}

//extern void kernel_power_off(void);
static void rk30_adc_battery_check(struct rk30_adc_battery_data *bat)
{
	int i;
	int level,oldlevel;
	struct rk30_adc_battery_platform_data *pdata = bat->pdata;
	//printk("%s--%d:\n",__FUNCTION__,__LINE__);

	bat->old_charge_level = -1;
	bat->capacitytmp = 0;
	bat->suspend_capacity = 0;
	
	pSamples = bat->adc_samples;

	adc_sync_read(bat->client);                             //start adc sample
	level = oldlevel = rk30_adc_battery_status_samples(bat);//init charge status

	bat->full_times = 0;
	for (i = 0; i < NUM_VOLTAGE_SAMPLE; i++){                //0.3 s
		mdelay(1);
		rk30_adc_battery_voltage_samples(bat);              //get voltage
		//level = rk30_adc_battery_status_samples(bat);       //check charge status
		level = rk30_adc_battery_get_charge_level(bat);

		if (oldlevel != level){
			oldlevel = level;                               //if charge status changed, reset sample
			i = 0;
		}        
	}

	bat->bat_capacity = rk30_adc_battery_voltage_to_capacity(bat, bat->bat_voltage);  //init bat_capacity

	
	bat->bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	if (rk30_adc_battery_get_charge_level(bat)){
		bat->bat_status = POWER_SUPPLY_STATUS_CHARGING;

		if (pdata->charge_ok_pin != INVALID_GPIO){
			if (gpio_get_value(pdata->charge_ok_pin) == pdata->charge_ok_level){
				bat->bat_status = POWER_SUPPLY_STATUS_FULL;
				bat->bat_capacity = 100;
			}
		}
	}

#if 0
	rk30_adc_battery_poweron_capacity_check();
#else
	gBatteryData->poweron_check = 1;
#endif
	gBatteryData->poweron_check = 1;

	if (bat->bat_capacity == 0) bat->bat_capacity = 1;


#if 0
	if ((bat->bat_voltage <= batt_table[0].dis_charge_vol + 50)
			&& (bat->bat_status != POWER_SUPPLY_STATUS_CHARGING)) {
		kernel_power_off();
	}
#endif
}

static void rk30_adc_battery_callback(struct adc_client *client, void *param, int result)
{
#if 0
	struct rk30_adc_battery_data  *info = container_of(client, struct rk30_adc_battery_data,
		client);
	info->adc_val = result;
#endif
	if (result < 0){
		pr_bat("adc_battery_callback    resule < 0 , the value ");
		return;
	}
	else{
		gBatteryData->adc_val = result;
		pr_bat("result = %d, gBatteryData->adc_val = %d\n", result, gBatteryData->adc_val );
	}
	return;
}

#if 1
static void rk30_adc_battery_lowerpower_delaywork(struct work_struct *work)
{
	int irq;
	if( gBatteryData->pdata->batt_low_pin != INVALID_GPIO){
		irq = gpio_to_irq(gBatteryData->pdata->batt_low_pin);
		disable_irq(irq);
	}

	printk("lowerpower\n");
	rk28_send_wakeup_key(); // wake up the system
	return;
}


static irqreturn_t rk30_adc_battery_low_wakeup(int irq,void *dev_id)
{
	queue_work(gBatteryData->wq, &gBatteryData->lowerpower_work);
	return IRQ_HANDLED;
}

#endif

static int rk30_adc_battery_probe(struct platform_device *pdev)
{
	int    ret;
	int    irq;
	int    irq_flag;
	struct adc_client				*client;
	struct rk30_adc_battery_data	*data;
	struct rk30_adc_battery_platform_data *pdata = pdev->dev.platform_data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}

	memset(data, 0, sizeof(struct rk30_adc_battery_data));
	gBatteryData = data;

	platform_set_drvdata(pdev, data);

	data->pdata = pdata;
	data->status_lock = 0;
	ret = rk30_adc_battery_io_init(pdata);
	if (ret) {
		goto err_io_init;
	}

	memset(data->adc_samples, 0, sizeof(int)*(NUM_VOLTAGE_SAMPLE + 2));

	 //register adc for battery sample
	client = adc_register(0, rk30_adc_battery_callback, NULL);  //pdata->adc_channel = ani0
	if(!client)
		goto err_adc_register_failed;
	    
	 //variable init
	data->client  = client;
	data->adc_val = adc_sync_read(client);

	ret = power_supply_register(&pdev->dev, &rk30_battery_supply);
	if (ret){
		printk(KERN_INFO "fail to battery power_supply_register\n");
		goto err_battery_failed;
	}


#if defined (CONFIG_BATTERY_RK30_USB_CHARGE)
	ret = power_supply_register(&pdev->dev, &rk30_usb_supply);
	if (ret){
		printk(KERN_INFO "fail to usb power_supply_register\n");
		goto err_usb_failed;
	}
#endif
	wake_lock_init(&batt_wake_lock, WAKE_LOCK_SUSPEND, "batt_lock");

	data->wq = create_singlethread_workqueue("adc_battd");
	INIT_DELAYED_WORK(&data->delay_work, rk30_adc_battery_timer_work);

	//Power on Battery detect
	rk30_adc_battery_check(data);
	queue_delayed_work(data->wq, &data->delay_work, msecs_to_jiffies(TIMER_MS_COUNTS + 8000));

#if  defined (CONFIG_BATTERY_RK30_AC_CHARGE)
	ret = power_supply_register(&pdev->dev, &rk30_ac_supply);
	if (ret) {
		printk(KERN_INFO "fail to ac power_supply_register\n");
		goto err_ac_failed;
	}
	//init dc dectet irq & delay work
	if (pdata->dc_det_pin != INVALID_GPIO){
		INIT_WORK(&data->dcwakeup_work, rk30_adc_battery_dcdet_delaywork);

		irq = gpio_to_irq(pdata->dc_det_pin);	        
		irq_flag = gpio_get_value (pdata->dc_det_pin) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
		ret = request_irq(irq, rk30_adc_battery_dc_wakeup, irq_flag, "ac_charge_irq", NULL);
		if (ret) {
			printk("failed to request dc det irq\n");
			goto err_dcirq_failed;
		}
		enable_irq_wake(irq);

	}
#endif

#if 1
	// batt low irq lowerpower_work
	if( pdata->batt_low_pin != INVALID_GPIO){
		INIT_WORK(&data->lowerpower_work, rk30_adc_battery_lowerpower_delaywork);

		irq = gpio_to_irq(pdata->batt_low_pin);
		ret = request_irq(irq, rk30_adc_battery_low_wakeup, IRQF_TRIGGER_LOW, "batt_low_irq", NULL);

		if (ret) {
			printk("failed to request batt_low_irq irq\n");
			goto err_lowpowerirq_failed;
		}
		disable_irq(irq);

	}
#endif

	printk(KERN_INFO "rk30_adc_battery: driver initialized\n");

	return 0;

#if defined (CONFIG_BATTERY_RK30_USB_CHARGE)
	err_usb_failed:
	power_supply_unregister(&rk30_usb_supply);
#endif

#if defined (CONFIG_BATTERY_RK30_AC_CHARGE)
	err_ac_failed:
	power_supply_unregister(&rk30_ac_supply);
#endif

	err_battery_failed:
	power_supply_unregister(&rk30_battery_supply);

	err_dcirq_failed:
	free_irq(gpio_to_irq(pdata->dc_det_pin), data);
#if 1
	err_lowpowerirq_failed:
	free_irq(gpio_to_irq(pdata->batt_low_pin), data);
#endif
	err_adc_register_failed:
	err_io_init:
	err_data_alloc_failed:
	kfree(data);

	printk("rk30_adc_battery: error!\n");

	return ret;
}

static int rk30_adc_battery_remove(struct platform_device *pdev)
{
	struct rk30_adc_battery_data *data = platform_get_drvdata(pdev);
	struct rk30_adc_battery_platform_data *pdata = pdev->dev.platform_data;

	cancel_delayed_work(&gBatteryData->delay_work);	
#if defined(CONFIG_BATTERY_RK30_USB_CHARGE)
	power_supply_unregister(&rk30_usb_supply);
#endif
#if defined(CONFIG_BATTERY_RK30_AC_CHARGE)
	power_supply_unregister(&rk30_ac_supply);
#endif
	power_supply_unregister(&rk30_battery_supply);

	free_irq(gpio_to_irq(pdata->dc_det_pin), data);

	kfree(data);
	
	return 0;
}

static struct platform_driver rk30_adc_battery_driver = {
	.probe		= rk30_adc_battery_probe,
	.remove		= rk30_adc_battery_remove,
	.suspend		= rk30_adc_battery_suspend,
	.resume		= rk30_adc_battery_resume,
	.driver = {
		.name = "rk30-battery",
		.owner	= THIS_MODULE,
	}
};

static int __init rk30_adc_battery_init(void)
{
	return platform_driver_register(&rk30_adc_battery_driver);
}

static void __exit rk30_adc_battery_exit(void)
{
	platform_driver_unregister(&rk30_adc_battery_driver);
}

subsys_initcall(rk30_adc_battery_init);//subsys_initcall(rk30_adc_battery_init);
module_exit(rk30_adc_battery_exit);

MODULE_DESCRIPTION("Battery detect driver for the rk30");
MODULE_AUTHOR("luowei lw@rock-chips.com");
MODULE_LICENSE("GPL");

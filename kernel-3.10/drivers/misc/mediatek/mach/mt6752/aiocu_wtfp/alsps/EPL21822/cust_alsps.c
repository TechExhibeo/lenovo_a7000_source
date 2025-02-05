#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
#include <mach/upmu_common.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 1,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .als_level  = { 0,  1,  1,   7,  15,  15,  100, 1000, 2000,  3000,  6000, 10000, 14000, 18000, 20000},
    .als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
    .ps_threshold_high = 28000,
    .ps_threshold_low = 23000,        
	/* lenovo-sw youwc1 20150122: adapter psensor for different project start */
    .ps_threshold_high_offset = 224,
    .ps_threshold_low_offset = 85,
	/* lenovo-sw youwc1 20150122: adapter psensor for different project end */
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}


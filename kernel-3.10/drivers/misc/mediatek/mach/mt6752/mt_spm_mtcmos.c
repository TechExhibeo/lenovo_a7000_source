#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>    //udelay
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_typedefs.h>
#include <mach/mt_spm_cpu.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <mach/hotplug.h>
#include <mach/mt_clkmgr.h>

#include <mach/md32_helper.h>
#include <mach/md32_ipi.h>
#include <mach/mt_pmic_wrap.h>

/**************************************
 * extern
 **************************************/
#ifdef CONFIG_MTK_L2C_SHARE
extern int IS_L2_BORROWED(void);
#endif //#ifdef CONFIG_MTK_L2C_SHARE


/**************************************
 * for CPU MTCMOS
 **************************************/
static DEFINE_SPINLOCK(spm_cpu_lock);
#ifdef CONFIG_OF
void __iomem *spm_cpu_base;
#endif //#ifdef CONFIG_OF

int spm_mtcmos_cpu_init(void)
{
#ifdef CONFIG_OF
    struct device_node *node;

    node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");
    if (!node) {
        pr_err("find SLEEP node failed\n");
        return -EINVAL;
    }
    spm_cpu_base = of_iomap(node, 0);
    if (!spm_cpu_base) {
        pr_err("base spm_cpu_base failed\n");
        return -EINVAL;
    }

    return 0;
#else //#ifdef CONFIG_OF
    return -EINVAL;
#endif //#ifdef CONFIG_OF
}

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
    spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
    spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}

typedef int (*spm_cpu_mtcmos_ctrl_func)(int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] =
{
    spm_mtcmos_ctrl_cpu0,
    spm_mtcmos_ctrl_cpu1,
    spm_mtcmos_ctrl_cpu2,
    spm_mtcmos_ctrl_cpu3,
    spm_mtcmos_ctrl_cpu4,
    spm_mtcmos_ctrl_cpu5,
    spm_mtcmos_ctrl_cpu6,
    spm_mtcmos_ctrl_cpu7
};
int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
    return (*spm_cpu_mtcmos_ctrl_funcs[cpu])(state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU0_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) | L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != CA7_CPU0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != CA7_CPU0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) & ~L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU1_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) | L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != CA7_CPU1) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != CA7_CPU1));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) & ~L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU2_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) | L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != CA7_CPU2) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != CA7_CPU2));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) & ~L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU3_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) | L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != CA7_CPU3) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != CA7_CPU3));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) & ~L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU0_STANDBYWFI) == 0);
            //mdelay(500);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) != CPU0_CA15_L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)))
        {
        #ifdef CONFIG_MTK_L2C_SHARE
            if (!IS_L2_BORROWED())
        #endif //#ifdef CONFIG_MTK_L2C_SHARE
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        }
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != CA15_CPU0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != CA15_CPU0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU1_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) != CPU1_CA15_L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)))
        {
        #ifdef CONFIG_MTK_L2C_SHARE
            if (!IS_L2_BORROWED())
        #endif //#ifdef CONFIG_MTK_L2C_SHARE
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        }
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != CA15_CPU1) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != CA15_CPU1));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU2_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) != CPU2_CA15_L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU3)))
        {
        #ifdef CONFIG_MTK_L2C_SHARE
            if (!IS_L2_BORROWED())
        #endif //#ifdef CONFIG_MTK_L2C_SHARE
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        }
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != CA15_CPU2) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != CA15_CPU2));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU3_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) != CPU3_CA15_L1_PDN_ACK);
    #endif //#ifndef CONFIG_MTK_FPGA
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2)))
        {
        #ifdef CONFIG_MTK_L2C_SHARE
            if (!IS_L2_BORROWED())
        #endif //#ifdef CONFIG_MTK_L2C_SHARE
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        }
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != CA15_CPU3) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != CA15_CPU3));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ISO);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN);
    #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) != 0);
    #endif //#ifndef CONFIG_MTK_FPGA
        udelay(1);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

#if 0 //There is no dbgsys wrapper in ca53 cpusys
int spm_mtcmos_ctrl_dbg0(int state)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_DBG_PWR_CON, (spm_read(SPM_CA7_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != CA7_DBG) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != CA7_DBG));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_CKISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_dbg1(int state)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ISO);
        spm_write(SPM_MP1_DBG_PWR_CON, (spm_read(SPM_MP1_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != MP1_DBG) || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != MP1_DBG));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_CKISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}
#endif //There is no dbgsys wrapper in ca53 cpusys

int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        //TODO: add per cpu power status check?
        
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPUTOP_STANDBYWFI) == 0);
        
        //XXX: no dbg0 mtcmos on k2
        //spm_mtcmos_ctrl_dbg0(state);

        //XXX: no async adb on k2
        //spm_topaxi_prot(CA7_PDN_REQ, 1);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_CKISO);
    #if 1
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) | L2_SRAM_PDN);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != L2_SRAM_PDN_ACK);
      #endif //#ifndef CONFIG_MTK_FPGA
        ndelay(1500);
    #else
        ndelay(100);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPUTOP_L2_SLEEP, spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & ~L2_SRAM_SLEEP_B);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) != 0);
      #endif //#ifndef CONFIG_MTK_FPGA
    #endif
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != CA7_CPUTOP) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != CA7_CPUTOP));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ISO);
        
    #if 1
        spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) & ~L2_SRAM_PDN);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != 0);
      #endif //#ifndef CONFIG_MTK_FPGA
    #else
        spm_write(SPM_CA7_CPUTOP_L2_SLEEP, spm_read(SPM_CA7_CPUTOP_L2_SLEEP) | L2_SRAM_SLEEP_B);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) != L2_SRAM_SLEEP_B_ACK);
      #endif //#ifndef CONFIG_MTK_FPGA
    #endif
        ndelay(900);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
        ndelay(100);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
        
        //XXX: no async adb on k2
        //spm_topaxi_prot(CA7_PDN_REQ, 0);
        
        //XXX: no dbg0 mtcmos on k2
        //spm_mtcmos_ctrl_dbg0(state);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        //XXX: no async adb on k2
        //spm_topaxi_prot(CA15_PDN_REQ, 1);
        
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPUTOP_STANDBYWFI) == 0);
            //mdelay(500);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
    #if 1
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != CA15_L2_PDN_ACK);
      #endif //#ifndef CONFIG_MTK_FPGA
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN_ISO);
        ndelay(1500);
    #else
        ndelay(100);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~L2_SRAM_SLEEP_B);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) != 0);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_SLEEPB_ISO);
      #endif //#ifndef CONFIG_MTK_FPGA
    #endif
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != 0));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON_2ND);
    #ifndef CONFIG_MTK_FPGA
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != CA15_CPUTOP) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != CA15_CPUTOP));
    #endif //#ifndef CONFIG_MTK_FPGA
                
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ISO);
        
    #if 1
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN_ISO);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != 0);
      #endif //#ifndef CONFIG_MTK_FPGA
    #else
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_SLEEPB_ISO);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | L2_SRAM_SLEEP_B);
      #ifndef CONFIG_MTK_FPGA
        while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) != L2_SRAM_SLEEP_B_ACK);
      #endif //#ifndef CONFIG_MTK_FPGA
    #endif
        ndelay(900);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
        ndelay(100);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
        
        //XXX: no async adb on k2
        //spm_topaxi_prot(CA15_PDN_REQ, 0);
    }
    
    return 0;
}

void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state)
{

    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_ctrl_cpu7(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu6(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu5(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu4(STA_POWER_DOWN, 0);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_ctrl_cpu4(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu5(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu6(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu7(STA_POWER_ON, 1);
        //spm_mtcmos_ctrl_dbg1(STA_POWER_ON);
    }

    //unsigned long flags;
    //
    ///* enable register control */
    //spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    //
    //spm_mtcmos_cpu_lock(&flags);
    //
    //spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);
    //
    //spm_mtcmos_cpu_unlock(&flags);
    //
    //spm_mtcmos_ctrl_dbg1(STA_POWER_ON);
}

bool spm_cpusys0_can_power_down(void)
{
    return !(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2 | CA7_CPU3)) &&
           !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2 | CA7_CPU3));
}

bool spm_cpusys1_can_power_down(void)
{
    return !(spm_read(SPM_PWR_STATUS) & (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) &&
           !(spm_read(SPM_PWR_STATUS_2ND) & (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 | CA15_CPU3));
}


/**************************************
 * for non-CPU MTCMOS
 **************************************/
static DEFINE_SPINLOCK(spm_noncpu_lock);

#if 0
void spm_mtcmos_noncpu_lock(unsigned long *flags)
{
    spin_lock_irqsave(&spm_noncpu_lock, *flags);
}

void spm_mtcmos_noncpu_unlock(unsigned long *flags)
{
    spin_unlock_irqrestore(&spm_noncpu_lock, *flags);
}
#else
#define spm_mtcmos_noncpu_lock(flags)   \
do {    \
    spin_lock_irqsave(&spm_noncpu_lock, flags);  \
} while (0)

#define spm_mtcmos_noncpu_unlock(flags) \
do {    \
    spin_unlock_irqrestore(&spm_noncpu_lock, flags);    \
} while (0)

#endif

#define MD2_PWR_STA_MASK    (0x1 << 27)
#define AUD_PWR_STA_MASK    (0x1 << 24)
#define MFG_ASYNC_PWR_STA_MASK (0x1 << 23)
#define VEN_PWR_STA_MASK    (0x1 << 21)
#define MJC_PWR_STA_MASK    (0x1 << 20) 
#define VDE_PWR_STA_MASK    (0x1 << 7)
//#define IFR_PWR_STA_MASK    (0x1 << 6)
#define ISP_PWR_STA_MASK    (0x1 << 5)
#define MFG_PWR_STA_MASK    (0x1 << 4)
#define DIS_PWR_STA_MASK    (0x1 << 3)
//#define DPY_PWR_STA_MASK    (0x1 << 2)
#define CONN_PWR_STA_MASK   (0x1 << 1)
#define MD1_PWR_STA_MASK    (0x1 << 0)

#if 0
#define PWR_RST_B           (0x1 << 0)
#define PWR_ISO             (0x1 << 1)
#define PWR_ON              (0x1 << 2)
#define PWR_ON_2ND          (0x1 << 3)
#define PWR_CLK_DIS         (0x1 << 4)
#endif

#define SRAM_PDN            (0xf << 8)
#define MFG_SRAM_PDN        (0x3f << 8) 
#define MD_SRAM_PDN         (0x1 << 8)
#define CONN_SRAM_PDN       (0x1 << 8)

#define VDE_SRAM_ACK        (0x1 << 12)
#define VEN_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0x1 << 12)
//#define MFG_SRAM_ACK        (0x3f << 16)
#define MFG_SRAM_ACK        (0x1 << 16)
#define MJC_SRAM_ACK        (0x1 << 12)
#define AUD_SRAM_ACK        (0xf << 12)

#define MD1_PROT_MASK        0x04B8//bit 3,4,5,7,10
#define MD2_PROT_MASK        0xF0000//16,17,18,19
#define DISP_PROT_MASK       0x0002//bit 1, if bit6 set, MMSYS PDN, access reg will hang
#define MFG_ASYNC_PROT_MASK  0xA00000//bit 21,23
#define CONN_PROT_MASK       0xE000//13,14,15


int spm_mtcmos_ctrl_vdec(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK) != VDE_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VDE_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VDE_PWR_CON, val);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
            	clk_stat_check(SYS_DIS);
                BUG();	
            }    
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_venc(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;    

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK) != VEN_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VEN_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VEN_PWR_CON, val);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_isp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK) != ISP_SRAM_ACK) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_ISP_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_ISP_PWR_CON, val);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_RST_B);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#if 0
int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
        }
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);
#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
        }
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        //val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        val = val | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#if 0
        udelay(1); 
        if (spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) { 
            err = 1;
        }
#else
        //while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
        //        || (spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
    } else {    /* STA_POWER_ON */
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);
#if 0
        udelay(1);
#else
        //while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
        //        || !(spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
        }
#endif

#if 0
        udelay(1); 
        if (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)) { 
            err = 1;
        }
#endif
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

#else

int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
            count++;
            if(count>1000)
                break;	
        }
        count = 0;
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#endif

int spm_mtcmos_ctrl_mfg(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;
    unsigned int rdata = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
//        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_2D_PROT_MASK);
//        while ((spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) != MFG_2D_PROT_MASK) {
//        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_PWR_CON, val);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
            count++;
            if (count > 1000 && count<1010) {
                pwrap_read(0x200, &rdata);
                printk("there is no VGPU, CID = 0x%x\n", rdata);
                pwrap_read(0x612, &rdata);
                printk("there is no VGPU, VGPU = 0x%x\n", rdata);
            }
            if (count > 2000)
                BUG();        	
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK)) {
        }

//        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_2D_PROT_MASK);
//        while (spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) {
//        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mfg_ASYNC(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_ASYNC_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MFG_ASYNC_PROT_MASK) != MFG_ASYNC_PROT_MASK) {
            count++;
            if(count>1000)
                break;
        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
//        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_ASYNC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_ASYNC_PWR_CON, val);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK)) {
//        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_ASYNC_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MFG_ASYNC_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

static volatile int DFS4MD_flag = 0;
//static volatile int count_down = 5000;

void DFS4MD_IPI_handler(int id, void *data, unsigned int len)
{
	//if (1 == *((int *)data))
	DFS4MD_flag = 1;
	return;
}

int spm_mtcmos_ctrl_mdsys1(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    volatile int count = 0;
    unsigned int data = 1;


    if (state == STA_POWER_DOWN) {

        spm_mtcmos_noncpu_lock(flags);
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MD1_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) != MD1_PROT_MASK) {
            count++;
            if(count>1000)
                break;
        }
        spm_mtcmos_noncpu_unlock(flags);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | MD_SRAM_PDN);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MD_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MD_PWR_CON, val);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }

    } else {    /* STA_POWER_ON */

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }

        if(is_md32_enable()){
            volatile ipi_status s = BUSY;
            DFS4MD_flag = 0;
            do{
                s = md32_ipi_send(IPI_DFS4MD, (void *)&data, sizeof(U32), 1);
                msleep(50);
                count++;
                if(count>40)
                    break;
		    }while(s != DONE);
            //        while(DFS4MD_flag == 0 || --count_down > 1);
        } else {
            spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_CLK_DIS);
            spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_ISO);
            spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_RST_B);
        }

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~MD_SRAM_PDN);

        spm_mtcmos_noncpu_lock(flags);
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MD1_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) {
        }
        spm_mtcmos_noncpu_unlock(flags);
    }

    return err;
}

int spm_mtcmos_ctrl_mdsys2(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MD2_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MD2_PROT_MASK) != MD2_PROT_MASK) {
            count++;
            if(count>1000)
                break;
        }

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) | MD_SRAM_PDN);

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MD2_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MD2_PWR_CON, val);

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MD2_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & MD2_PWR_STA_MASK)) {
        }

    } else {    /* STA_POWER_ON */

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) | PWR_ON);
        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MD2_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & MD2_PWR_STA_MASK)) {
        }

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MD2_PWR_CON, spm_read(SPM_MD2_PWR_CON) & ~MD_SRAM_PDN);

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MD2_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MD2_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_connsys(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | CONN_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & CONN_PROT_MASK) != CONN_PROT_MASK) {
            count++;
            if(count>1000)	
                break;	
        }

        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) | CONN_SRAM_PDN);

        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_CONN_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_CONN_PWR_CON, val);

        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & CONN_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
        }
    } else {    
        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) | PWR_ON);
        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & CONN_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
        }

        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) | PWR_RST_B);

        spm_write(SPM_CONN_PWR_CON, spm_read(SPM_CONN_PWR_CON) & ~CONN_SRAM_PDN);

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~CONN_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & CONN_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mjc(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK) != MJC_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MJC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MJC_PWR_CON, val);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();	
        }
    }
 
    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_aud(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK) != AUD_SRAM_ACK) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_AUDIO_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_AUDIO_PWR_CON, val);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_RST_B);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~SRAM_PDN);
        
        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_topaxi_prot(int bit, int en)
{
    unsigned long flags;	
    spm_mtcmos_noncpu_lock(flags);

    if (en == 1) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | (1<<bit));
        while ((spm_read(TOPAXI_PROT_STA1) & (1<<bit)) != (1<<bit)) {
        }
    } else {
   	    spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~(1<<bit));
        while (spm_read(TOPAXI_PROT_STA1) & (1<<bit)) {
        }
    }    

    spm_mtcmos_noncpu_unlock(flags);    

    return 0;
}
static int mt_spm_mtcmos_init(void)
{
	md32_ipi_registration(IPI_DFS4MD, DFS4MD_IPI_handler, "DFS4MD_IPI_handler");
	return 0;
}

module_init(mt_spm_mtcmos_init);

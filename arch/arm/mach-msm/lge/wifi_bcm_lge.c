#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <asm/io.h>
#ifdef CONFIG_WIFI_CONTROL_FUNC
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif
#include <mach/board_lge.h> // add for hw revision check by hayun.kim


#define WLAN_POWER    46
#if defined(CONFIG_MACH_MSM8926_X10_VZW) || defined(CONFIG_MACH_MSM8926_JAGC_SPR)
#define WLAN_HOSTWAKE 56
#elif defined(CONFIG_MACH_MSM8926_JAGDSNM_CMCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CUCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CTC_CN)
#define WLAN_HOSTWAKE 35
#else
#define WLAN_HOSTWAKE 67
#endif

static int gpio_wlan_power = WLAN_POWER; // add for hw revision check by hayun.kim
static int gpio_wlan_hostwake = WLAN_HOSTWAKE; // add for hw revision check by hayun.kim

static unsigned wlan_wakes_msm[] = {
	    GPIO_CFG(WLAN_HOSTWAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA) };

/* for wifi power supply */
static unsigned wifi_config_power_on[] = {
	    GPIO_CFG(WLAN_POWER, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };

#if defined(CONFIG_BCM4335BT) 
extern int bcm_bt_lock(int cookie);
extern void bcm_bt_unlock(int cookie);
static int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24; /* cookie is "WiFi" */
#endif // defined(CONFIG_BCM4335BT) 

// For broadcom
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
}
	wlan_static_scan_buf0 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf0)
		goto err_mem_alloc;
	wlan_static_scan_buf1 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf1)
		goto err_mem_alloc;

	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

static unsigned int g_wifi_detect;
static void *sdc3_dev;
void (*sdc3_status_cb)(int card_present, void *dev);

int sdc3_status_register(void (*cb)(int card_present, void *dev), void *dev)
{

	printk(KERN_ERR "%s: Dubugging Point 1\n", __func__);

	if(sdc3_status_cb) {
		return -EINVAL;
	}
	sdc3_status_cb = cb;
	sdc3_dev = dev;
	return 0;
}

unsigned int sdc3_status(struct device *dev)
{
	printk("J:%s> g_wifi_detect = %d\n", __func__, g_wifi_detect );
	return g_wifi_detect;
}
#if 1
int bcm_wifi_reinit_gpio( void )
{
	int rc=0;

	int hw_rev = HW_REV_A;

	// set gpio value
	hw_rev = lge_get_board_revno();		

//	gpio_wlan_hostwake 	= WLAN_HOSTWAKE;
//	gpio_wlan_power 	= WLAN_POWER;

	printk(KERN_ERR "%s: rev=%d, gpio_power=%d, gpio_hostwakeup=%d \n", __func__, hw_rev, gpio_wlan_power, gpio_wlan_hostwake);

	// COMMON
	wlan_wakes_msm[0] = GPIO_CFG(gpio_wlan_hostwake, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
	wifi_config_power_on[0] = GPIO_CFG(gpio_wlan_power, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		
	//WLAN_POWER
	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	//HOST_WAKEUP
	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);

	if (gpio_direction_input(gpio_wlan_hostwake)) 
		printk(KERN_ERR "%s: WL_HOSTWAKE failed direction in\n", __func__);

	return 0;
}
#endif
int bcm_wifi_set_power(int enable)
{
	int ret = 0;
#if 1    
	static int is_initialized = 0;

	if (is_initialized == 0) {
		bcm_wifi_reinit_gpio();
		mdelay(10);
		is_initialized = 1;
	}
#endif	
#if defined(CONFIG_BCM4335BT) 
	printk("%s: trying to acquire BT lock\n", __func__);
	if (bcm_bt_lock(lock_cookie_wifi) != 0)
		printk("%s:** WiFi: timeout in acquiring bt lock**\n", __func__);
	else 
		printk("%s: btlock acquired\n", __func__);
#endif // defined(CONFIG_BCM4335BT) 

	if (enable)
	{
		ret = gpio_direction_output(gpio_wlan_power, 1); 
		if (ret) 
		{
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}

		// WLAN chip to reset
		mdelay(150); //for booting time save
		printk("J:%s: applied delay. 150ms\n",__func__);
		printk(KERN_ERR "%s: wifi power successed to pull up\n",__func__);

	}
	else{
		ret = gpio_direction_output(gpio_wlan_power, 0); 
		if (ret) 
		{
			printk(KERN_ERR "%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}

		// WLAN chip down 
		mdelay(100);//for booting time save
		printk(KERN_ERR "%s: wifi power successed to pull down\n",__func__);
	}

#if defined(CONFIG_BCM4335BT) 
	bcm_bt_unlock(lock_cookie_wifi);
#endif // defined(CONFIG_BCM4335BT) 

	return ret;

out : 
#if defined(CONFIG_BCM4335BT) 
	/* For a exceptional case, release btlock */
	printk("%s: exceptional bt_unlock\n", __func__);
	bcm_bt_unlock(lock_cookie_wifi);
#endif // defined(CONFIG_BCM4335BT) 

	return ret;
}

int __init bcm_wifi_init_gpio_mem( struct platform_device* platdev )
{
	int rc=0;
#if 1 
// add for hw revision check by hayun.kim, START
	int hw_rev = HW_REV_A;

	hw_rev = lge_get_board_revno();		
	
//	gpio_wlan_hostwake 	= WLAN_HOSTWAKE;
//	gpio_wlan_power 	= WLAN_POWER;

	printk(KERN_ERR "%s: rev=%d, gpio_power=%d, gpio_hostwakeup=%d \n", __func__, hw_rev, gpio_wlan_power, gpio_wlan_hostwake);

	// COMMON
	wlan_wakes_msm[0] = GPIO_CFG(gpio_wlan_hostwake, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
	wifi_config_power_on[0] = GPIO_CFG(gpio_wlan_power, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
// add for hw revision check by hayun.kim, END
#endif

	//WLAN_POWER
	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	if (gpio_request(gpio_wlan_power, "WL_REG_ON"))		
		printk("Failed to request gpio %d for WL_REG_ON\n", gpio_wlan_power);	

	if (gpio_direction_output(gpio_wlan_power, 0)) 
		printk(KERN_ERR "%s: WL_REG_ON  failed direction out\n", __func__);

	//HOST_WAKEUP
	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);
	if (gpio_request(gpio_wlan_hostwake, "wlan_wakes_msm"))		
		printk("Failed to request gpio %d for wlan_wakes_msm\n", gpio_wlan_hostwake);			
	if (gpio_direction_input(gpio_wlan_hostwake)) 
		printk(KERN_ERR "%s: WL_HOSTWAKE failed direction in\n", __func__);


	//For MSM8974_S
	if( platdev != NULL )
	{
		struct resource* resource = platdev->resource;
		if( resource != NULL )
		{
			resource->start = gpio_to_irq( gpio_wlan_hostwake );
			resource->end = gpio_to_irq( gpio_wlan_hostwake ); 

			printk("J:%s> resource->start = %d\n", __func__, resource->start );
		}
	}
	//For MSM8974_E

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	printk("bcm_wifi_init_gpio_mem successfully \n");

	return 0;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;
	if(sdc3_status_cb)
		sdc3_status_cb(val, sdc3_dev);
	else
		printk("%s:There is no callback for notify\n", __FUNCTION__);
	return 0;
}

static int bcm_wifi_get_mac_addr(unsigned char* buf)
{
	uint rand_mac;
	static unsigned char mymac[6] = {0,};
	const unsigned char nullmac[6] = {0,};
	pr_debug("%s: %p\n", __func__, buf);

	if( buf == NULL ) return -EAGAIN;

	if( memcmp( mymac, nullmac, 6 ) != 0 )
	{
		/* Mac displayed from UI are never updated..
		   So, mac obtained on initial time is used */
		memcpy( buf, mymac, 6 );
		return 0;
	}

	srandom32((uint)jiffies);
	rand_mac = random32();
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy( mymac, buf, 6 );

	printk("[%s] Exiting. MyMac :  %x : %x : %x : %x : %x : %x \n",__func__ , buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

	return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement BCM4334 series */
           {"",       "GB",     0},
           {"AD",    "GB",     0},
           {"AE",    "KR",     24},
           {"AF",    "AF",     0},
           {"AG",    "US",     100},
           {"AI",     "US",     100},
           {"AL",    "GB",     0},
           {"AM",   "IL",      10},
           {"AN",   "BR",     0},
           {"AO",   "IL",      10},
           {"AR",    "BR",     0},
           {"AS",    "US",     100},
           {"AT",    "GB",     0},
           {"AU",    "AU",    2},
           {"AW",   "KR",     24},
           {"AZ",    "BR",     0},
           {"BA",    "GB",     0},
           {"BB",    "RU",     1},
           {"BD",    "CN",    0},
           {"BE",    "GB",     0},
           {"BF",    "CN",    0},
           {"BG",    "GB",     0},
           {"BH",    "RU",     1},
           {"BI",     "IL",      10},
           {"BJ",     "IL",      10},
           {"BM",   "US",     100},
           {"BN",    "RU",     1},
           {"BO",    "IL",      10},
           {"BR",    "BR",     0},
           {"BS",    "RU",     1},
           {"BT",    "IL",      10},
           {"BW",   "GB",     0},
           {"BY",    "GB",     0},
           {"BZ",    "IL",      10},
           {"CA",    "US",     100},
           {"CD",    "IL",      10},
           {"CF",    "IL",      10},
           {"CG",    "IL",      10},
           {"CH",    "GB",     0},
           {"CI",     "IL",      10},
           {"CK",    "BR",     0},
           {"CL",    "RU",     1},
           {"CM",   "IL",      10},
           {"CN",    "CN",    0},
           {"CO",    "BR",     0},
           {"CR",    "BR",     0},
           {"CU",    "BR",     0},
           {"CV",    "GB",     0},
           {"CX",    "AU",    2},
           {"CY",    "GB",     0},
           {"CZ",    "GB",     0},
           {"DE",    "GB",     0},
           {"DJ",    "IL",      10},
           {"DK",    "GB",     0},
           {"DM",   "BR",     0},
           {"DO",   "BR",     0},
           {"DZ",    "KW",    1},
           {"EC",    "BR",     0},
           {"EE",    "GB",     0},
           {"EG",    "RU",     1},
           {"ER",    "IL",      10},
           {"ES",    "GB",     0},
           {"ET",    "GB",     0},
           {"FI",     "GB",     0},
           {"FJ",     "IL",      10},
           {"FM",   "US",     100},
           {"FO",    "GB",     0},
           {"FR",    "GB",     0},
           {"GA",    "IL",      10},
           {"GB",    "GB",     0},
           {"GD",    "BR",     0},
           {"GE",    "GB",     0},
           {"GF",    "GB",     0},
           {"GH",    "BR",     0},
           {"GI",     "GB",     0},
           {"GM",   "IL",      10},
           {"GN",   "IL",      10},
           {"GP",    "GB",     0},
           {"GQ",   "IL",      10},
           {"GR",    "GB",     0},
           {"GT",    "RU",     1},
           {"GU",    "US",     100},
           {"GW",   "IL",      10},
           {"GY",    "QA",    0},
           {"HK",    "BR",     0},
           {"HN",   "CN",    0},
           {"HR",    "GB",     0},
           {"HT",    "RU",     1},
           {"HU",    "GB",     0},
           {"ID",     "QA",    0},
           {"IE",     "GB",     0},
           {"IL",     "IL",      10},
           {"IM",    "GB",     0},
           {"IN",    "RU",     1},
           {"IQ",    "IL",      10},
           {"IR",     "IL",      10},
           {"IS",     "GB",     0},
           {"IT",     "GB",     0},
           {"JE",     "GB",     0},
           {"JM",    "GB",     0},
           {"JP",     "JP",      5},
           {"KE",    "GB",     0},
           {"KG",    "IL",      10},
           {"KH",    "BR",     0},
           {"KI",     "AU",    2},
           {"KM",   "IL",      10},
           {"KP",    "IL",      10},
           {"KR",    "KR",     24},
           {"KW",   "KW",    1},
           {"KY",    "US",     100},
           {"KZ",    "BR",     0},
           {"LA",    "KR",     24},
           {"LB",    "BR",     0},
           {"LC",    "BR",     0},
           {"LI",     "GB",     0},
           {"LK",    "BR",     0},
           {"LR",    "BR",     0},
           {"LS",     "GB",     0},
           {"LT",     "GB",     0},
           {"LU",    "GB",     0},
           {"LV",     "GB",     0},
           {"LY",     "IL",      10},
           {"MA",   "KW",    1},
           {"MC",   "GB",     0},
           {"MD",   "GB",     0},
           {"ME",   "GB",     0},
           {"MF",   "GB",     0},
           {"MG",   "IL",      10},
           {"MH",   "BR",     0},
           {"MK",   "GB",     0},
           {"ML",    "IL",      10},
           {"MM",  "IL",      10},
           {"MN",   "IL",      10},
           {"MO",   "CN",    0},
           {"MP",   "US",     100},
           {"MQ",   "GB",     0},
           {"MR",   "GB",     0},
           {"MS",   "GB",     0},
           {"MT",   "GB",     0},
           {"MU",   "GB",     0},
           {"MD",   "GB",     0},
           {"ME",   "GB",     0},
           {"MF",   "GB",     0},
           {"MG",   "IL",      10},
           {"MH",   "BR",     0},
           {"MK",   "GB",     0},
           {"ML",    "IL",      10},
           {"MM",  "IL",      10},
           {"MN",   "IL",      10},
           {"MO",   "CN",    0},
           {"MP",   "US",     100},
           {"MQ",   "GB",     0},
           {"MR",   "GB",     0},
           {"MS",   "GB",     0},
           {"MT",   "GB",     0},
           {"MU",   "GB",     0},
           {"MV",   "RU",     1},
           {"MW",  "CN",    0},
           {"MX",   "RU",     1},
           {"MY",   "RU",     1},
           {"MZ",   "BR",     0},
           {"NA",   "BR",     0},
           {"NC",    "IL",      10},
           {"NE",    "BR",     0},
           {"NF",    "BR",     0},
           {"NG",   "NG",    0},
           {"NI",    "BR",     0},
           {"NL",    "GB",     0},
           {"NO",   "GB",     0},
           {"NP",    "SA",     0},
           {"NR",    "IL",      10},
           {"NU",   "BR",     0},
           {"NZ",    "BR",     0},
           {"OM",   "GB",     0},
           {"PA",    "RU",     1},
           {"PE",    "BR",     0},
           {"PF",    "GB",     0},
           {"PH",    "BR",     0},
           {"PK",    "CN",    0},
           {"PL",     "GB",     0},
           {"PM",   "GB",     0},
           {"PN",    "GB",     0},
           {"PR",    "US",     100},
           {"PS",    "BR",     0},
           {"PT",    "GB",     0},
           {"PW",   "BR",     0},
           {"PY",    "BR",     0},
           {"QA",   "CN",    0},
           {"RE",    "GB",     0},
           {"RKS",   "IL",     10},
           {"RO",    "GB",     0},
           {"RS",    "GB",     0},
           {"RU",    "RU",     10},
           {"RW",   "CN",    0},
           {"SA",    "SA",     0},
           {"SB",    "IL",      10},
           {"SC",    "IL",      10},
           {"SD",    "GB",     0},
           {"SE",    "GB",     0},
           {"SG",    "BR",     0},
           {"SI",     "GB",     0},
           {"SK",    "GB",     0},
           {"SKN",  "CN",   0},
           {"SL",     "IL",      10},
           {"SM",   "GB",     0},
           {"SN",    "GB",     0},
           {"SO",    "IL",      10},
           {"SR",    "IL",      10},
           {"SS",    "GB",     0},
           {"ST",    "IL",      10},
           {"SV",    "RU",     1},
           {"SY",    "BR",     0},
           {"SZ",    "IL",      10},
           {"TC",    "GB",     0},
           {"TD",    "IL",      10},
           {"TF",    "GB",     0},
           {"TG",    "IL",      10},
           {"TH",    "BR",     0},
           {"TJ",     "IL",      10},
           {"TL",     "BR",     0},
           {"TM",   "IL",      10},
           {"TN",    "KW",    1},
           {"TO",    "IL",      10},
           {"TR",    "GB",     0},
           {"TT",    "BR",     0},
           {"TV",    "IL",      10},
           {"TW",   "TW",    2},
           {"TZ",    "CN",    0},
           {"UA",    "RU",     1},
           {"UG",    "BR",     0},
           {"US",    "US",     100},
           {"UY",    "BR",     0},
           {"UZ",    "IL",      10},
           {"VA",    "GB",     0},
           {"VC",    "BR",     0},
           {"VE",    "RU",     1},
           {"VG",    "GB",     0},
           {"VI",     "US",     100},
           {"VN",    "BR",     0},
           {"VU",    "IL",      10},
           {"WS",   "SA",     0},
           {"YE",    "IL",      10},
           {"YT",    "GB",     0},
           {"ZA",    "GB",     0},
           {"ZM",   "RU",     1},
           {"ZW",   "BR",     0},
};

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;
	static struct cntry_locales_custom country_code;

	size = ARRAY_SIZE(bcm_wifi_translate_custom_table);

	if ((size == 0) || (ccode == NULL))
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table[i].iso_abbrev) == 0) {
			return (void *)&bcm_wifi_translate_custom_table[i];
		}
	}   

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return (void *)&country_code;
}

static struct wifi_platform_data bcm_wifi_control = {
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, 
	.get_country_code = bcm_wifi_get_country_code,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  //assigned later
		.end   = 0,  //assigned later
		//.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, // for HW_OOB
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_SHAREABLE, // for SW_OOB
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};

void __init init_bcm_wifi(void)
{
#ifdef CONFIG_WIFI_CONTROL_FUNC
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);
#endif
}


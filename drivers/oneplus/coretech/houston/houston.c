#define pr_fmt(fmt) "houston: " fmt

#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/oneplus/houston.h>

static int log_lv = 0;
module_param(log_lv, int, 0664);

/* ais */
static int ais_enable = 0;
module_param(ais_enable, int, 0664);

static bool cpuload_query = false;
module_param(cpuload_query, bool, 0664);

/* battery query, it takes time to query */
static bool bat_query = false;
module_param(bat_query, bool, 0664);

static bool bat_sample_high_resolution = false;
module_param(bat_sample_high_resolution, bool, 0664);

/* force update battery current */
static unsigned long bat_update_period_us = 1000000; // 1 sec
module_param(bat_update_period_us, ulong, 0664);

/* fps boost switch */
static bool fps_boost_enable = false;
module_param(fps_boost_enable, bool, 0664);

/* freq hispeed */
static bool cpufreq_hispeed_enable = false;
module_param_named(cpufreq_hispeed_enable, cpufreq_hispeed_enable, bool, 0664);

static unsigned int cpufreq_hispeed[3] = { 1209600, 1612800, 1612800 };
module_param_array(cpufreq_hispeed, uint, NULL, 0664);

static bool ddrfreq_hispeed_enable = false;
module_param(ddrfreq_hispeed_enable, bool, 0664);

static unsigned int ddrfreq_hispeed = 1017;
module_param(ddrfreq_hispeed, uint, 0664);

/* choose boost freq to lock or lower bound */
static unsigned int fps_boost_type = 1;
module_param(fps_boost_type, uint, 0664);

/* filter out too close boost hint */
static unsigned long fps_boost_filter_us = 8000;
module_param(fps_boost_filter_us, ulong, 0664);

static int perf_ready = -11;
module_param(perf_ready, int, 0664);

unsigned int ht_enable = 0;
module_param(ht_enable, uint, 0664);

unsigned int sample_rate_ms = 0;
module_param(sample_rate_ms, uint, 0664);

unsigned int fps_data_sync = 0;
module_param(fps_data_sync, uint, 0664);

static int ht_init(void)
{
	return 0;
}
pure_initcall(ht_init);

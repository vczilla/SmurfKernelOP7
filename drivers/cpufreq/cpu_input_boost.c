// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2019 Sultan Alsawaf <sultan@kerneltoast.com>.
 */
// Copyright (C) 2019 GPU input boost, stune boost, Erik Müller <pappschlumpf@xda>

#define pr_fmt(fmt) "cpu_input_boost: " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#else
#include <linux/fb.h>
#endif
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/cpu_input_boost.h>
#include <linux/power_hal.h>
#include "../gpu/msm/kgsl.h"
#include "../gpu/msm/kgsl_pwrscale.h"
#include "../gpu/msm/kgsl_device.h"
#include <linux/version.h>

/* The sched_param struct is located elsewhere in newer kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#include <uapi/linux/sched/types.h>
#endif

static unsigned int input_boost_freq_lp __read_mostly = CONFIG_INPUT_BOOST_FREQ_LP;
static unsigned int input_boost_freq_hp __read_mostly = CONFIG_INPUT_BOOST_FREQ_PERF;
static unsigned int input_boost_freq_gold __read_mostly = CONFIG_INPUT_BOOST_FREQ_GOLD;
static unsigned int flex_boost_freq_lp __read_mostly = CONFIG_FLEX_BOOST_FREQ_LP;
static unsigned int flex_boost_freq_hp __read_mostly = CONFIG_FLEX_BOOST_FREQ_PERF;
static unsigned int flex_boost_freq_gold __read_mostly = CONFIG_FLEX_BOOST_FREQ_GOLD;
static unsigned int flex_boost_freq_lp_1 __read_mostly = CONFIG_FLEX_BOOST_FREQ_LP_1;
static unsigned int flex_boost_freq_hp_1 __read_mostly = CONFIG_FLEX_BOOST_FREQ_PERF_1;
static unsigned int flex_boost_freq_gold_1 __read_mostly = CONFIG_FLEX_BOOST_FREQ_GOLD_1;
static unsigned int max_boost_freq_lp __read_mostly = CONFIG_MAX_BOOST_FREQ_LP;
static unsigned int max_boost_freq_hp __read_mostly = CONFIG_MAX_BOOST_FREQ_PERF;
static unsigned int max_boost_freq_gold __read_mostly = CONFIG_MAX_BOOST_FREQ_GOLD;
static unsigned int max_boost_freq_lp_1 __read_mostly = CONFIG_MAX_BOOST_FREQ_LP_1;
static unsigned int max_boost_freq_hp_1 __read_mostly = CONFIG_MAX_BOOST_FREQ_PERF_1;
static unsigned int max_boost_freq_gold_1 __read_mostly = CONFIG_MAX_BOOST_FREQ_GOLD_1;
static unsigned int max_boost_freq_lp_2 __read_mostly = CONFIG_MAX_BOOST_FREQ_LP_2;
static unsigned int max_boost_freq_hp_2 __read_mostly = CONFIG_MAX_BOOST_FREQ_PERF_2;
static unsigned int max_boost_freq_gold_2 __read_mostly = CONFIG_MAX_BOOST_FREQ_GOLD_2;
static unsigned int remove_input_boost_freq_lp __read_mostly = CONFIG_REMOVE_INPUT_BOOST_FREQ_LP;
static unsigned int remove_input_boost_freq_perf __read_mostly = CONFIG_REMOVE_INPUT_BOOST_FREQ_PERF;
static unsigned int remove_input_boost_freq_gold __read_mostly = CONFIG_REMOVE_INPUT_BOOST_FREQ_GOLD;
unsigned int sleep_freq_lp __read_mostly = 576000;
unsigned int sleep_freq_hp __read_mostly = 710400;
unsigned int sleep_freq_gold __read_mostly = 825600;
static unsigned int gpu_boost_freq __read_mostly = CONFIG_GPU_BOOST_FREQ;
static unsigned int gpu_min_freq __read_mostly = CONFIG_GPU_MIN_FREQ;
static unsigned int gpu_sleep_freq __read_mostly = 180; 
static unsigned short input_boost_duration __read_mostly = CONFIG_INPUT_BOOST_DURATION_MS;
static unsigned short flex_boost_duration __read_mostly = CONFIG_FLEX_BOOST_DURATION_MS;
static unsigned int input_thread_prio __read_mostly = CONFIG_INPUT_THREAD_PRIORITY;
static unsigned int gpu_boost_extender_ms __read_mostly = CONFIG_GPU_BOOST_EXTENDER_MS;
static bool little_only __read_mostly = false;
bool boost_gold __read_mostly = true;
static bool gpu_oc __read_mostly = false;
static bool ufs_boost __read_mostly = false;
static bool smumode = true;

unsigned int base_stune_boost __read_mostly = 20;
unsigned int default_level_stune_boost __read_mostly = 5;
static short input_stune_boost_offset __read_mostly = CONFIG_INPUT_BOOST_STUNE_OFFSET;
static short max_stune_boost_offset __read_mostly = CONFIG_MAX_BOOST_STUNE_OFFSET;
static short flex_stune_boost_offset __read_mostly = CONFIG_FLEX_BOOST_STUNE_OFFSET;
static unsigned int stune_boost_extender_ms __read_mostly = CONFIG_STUNE_BOOST_EXTENDER_MS;
static unsigned int max_stune_boost_extender_ms __read_mostly = CONFIG_MAX_STUNE_BOOST_EXTENDER_MS;
static unsigned int sleep_level_stune_boost __read_mostly = 1;

static unsigned int gpu_prev_freq=257;

enum {
	SCREEN_ON,
	INPUT_BOOST,
	FLEX_BOOST,
	CLUSTER1_BOOST,
	CLUSTER2_BOOST,
	CORE_BOOST,
	CLUSTER1_WAKE_BOOST,
	CLUSTER2_WAKE_BOOST,
	INPUT_STUNE_BOOST,
	MAX_STUNE_BOOST,
	FLEX_STUNE_BOOST,
	UFS_BOOST,
	GPU_INPUT_BOOST,
	GPU_FLEX_BOOST,
};

struct boost_drv *gov_cpu_state = NULL;

module_param(default_level_stune_boost, uint, 0644);
module_param(base_stune_boost, uint, 0644);
module_param(input_stune_boost_offset, short, 0644);
module_param(max_stune_boost_offset, short, 0644);
module_param(flex_stune_boost_offset, short, 0644);
module_param(gpu_boost_freq, uint, 0644);
module_param(gpu_min_freq, uint, 0644);
module_param(gpu_sleep_freq, uint, 0644);
module_param(max_stune_boost_extender_ms, uint, 0644);
module_param(stune_boost_extender_ms, uint, 0644);

module_param(input_boost_freq_lp, uint, 0644);
module_param(input_boost_freq_hp, uint, 0644);
module_param(input_boost_freq_gold, uint, 0644);
module_param(flex_boost_freq_lp, uint, 0644);
module_param(flex_boost_freq_hp, uint, 0644);
module_param(flex_boost_freq_gold, uint, 0644);
module_param(remove_input_boost_freq_lp, uint, 0644);
module_param(remove_input_boost_freq_perf, uint, 0644);
module_param(remove_input_boost_freq_gold, uint, 0644);
module_param(max_boost_freq_lp, uint, 0644);
module_param(max_boost_freq_hp, uint, 0644);
module_param(max_boost_freq_gold, uint, 0644);
module_param(input_boost_duration, short, 0644);
module_param(flex_boost_duration, short, 0644);
module_param(gpu_boost_extender_ms, uint, 0644);
module_param(little_only, bool, 0644);
module_param(boost_gold, bool, 0644);
module_param(gpu_oc, bool, 0644);
module_param(ufs_boost, bool, 0644);
module_param(sleep_freq_lp, uint, 0644);
module_param(sleep_freq_hp, uint, 0644);
module_param(sleep_freq_gold, uint, 0644);

static void input_unboost_worker(struct work_struct *work);
static void flex_unboost_worker(struct work_struct *work);
static void ufs_unboost_worker(struct work_struct *work);
static void cluster1_unboost_worker(struct work_struct *work);
static void cluster2_unboost_worker(struct work_struct *work);
static void input_stune_unboost_worker(struct work_struct *work);
static void max_stune_unboost_worker(struct work_struct *work);
static void gpu_unboost_worker(struct work_struct *work);
static void gpu_flex_unboost_worker(struct work_struct *work);
static void core_unboost_worker(struct work_struct *work);
static void flex_stune_unboost_worker(struct work_struct *work);

static struct boost_drv boost_drv_g __read_mostly = {
	.input_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.input_unboost,
						  input_unboost_worker, 0),
	.flex_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.flex_unboost,
						  flex_unboost_worker, 0),
	.core_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.core_unboost,
						  core_unboost_worker, 0),
	.ufs_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.ufs_unboost,
						  ufs_unboost_worker, 0),
	.cluster1_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.cluster1_unboost,
						  cluster1_unboost_worker, 0),
	.cluster2_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.cluster2_unboost,
						  cluster2_unboost_worker, 0),
	.input_stune_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.input_stune_unboost,
						  input_stune_unboost_worker, 0),
	.max_stune_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.max_stune_unboost,
						  max_stune_unboost_worker, 0),
	.flex_stune_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.flex_stune_unboost,
						  flex_stune_unboost_worker, 0),
	.gpu_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.gpu_unboost,
						  gpu_unboost_worker, 0),
	.gpu_flex_unboost = __DELAYED_WORK_INITIALIZER(boost_drv_g.gpu_flex_unboost,
						  gpu_flex_unboost_worker, 0),
	.cpu_boost_waitq = __WAIT_QUEUE_HEAD_INITIALIZER(boost_drv_g.cpu_boost_waitq),
	.gpu_boost_waitq = __WAIT_QUEUE_HEAD_INITIALIZER(boost_drv_g.gpu_boost_waitq),
	.stune_boost_waitq = __WAIT_QUEUE_HEAD_INITIALIZER(boost_drv_g.stune_boost_waitq)
};

unsigned int get_input_boost_freq(struct cpufreq_policy *policy)
{
	if (cpumask_test_cpu(policy->cpu, cpu_lp_mask))
		return input_boost_freq_lp;
	else if (cpumask_test_cpu(policy->cpu, cpu_perf_mask))
		return input_boost_freq_hp;
	return  input_boost_freq_gold; 
}

unsigned int get_max_boost_freq(struct cpufreq_policy *policy, int stage)
{
	if (cpumask_test_cpu(policy->cpu, cpu_lp_mask)) {
		switch (stage) {
			case 0: return max_boost_freq_lp;
				break;
			case 1: return max_boost_freq_lp_1;
				break;
			case 2: return max_boost_freq_lp_2;
				break;
		}
	} else if (cpumask_test_cpu(policy->cpu, cpu_perf_mask)) {
		switch (stage) {
			case 0: return max_boost_freq_hp;
				break;
			case 1: return max_boost_freq_hp_1;
				break;
			case 2: return max_boost_freq_hp_2;
				break;
		}
	}
	switch (stage) {
		case 0: return max_boost_freq_gold;
			break;
		case 1: return max_boost_freq_gold_1;
			break;
		case 2: return max_boost_freq_gold_2;
			break;
	}
}

unsigned int get_flex_boost_freq(struct cpufreq_policy *policy, int stage)
{
	if (cpumask_test_cpu(policy->cpu, cpu_lp_mask)) {
		switch (stage) {
			case 0: return flex_boost_freq_lp;
				break;
			case 1: return flex_boost_freq_lp_1;
				break;
		}
	} else if (cpumask_test_cpu(policy->cpu, cpu_perf_mask)) {
		switch (stage) {
			case 0: return flex_boost_freq_hp;
				break;
			case 1: return flex_boost_freq_hp_1;
				break;
		}
	}
	switch (stage) {
		case 0: return flex_boost_freq_gold;
			break;
		case 1: return flex_boost_freq_gold_1;
			break;
	}
}

unsigned int get_min_freq(struct cpufreq_policy *policy)
{
	if (cpumask_test_cpu(policy->cpu, cpu_lp_mask))
		return remove_input_boost_freq_lp;
	if (cpumask_test_cpu(policy->cpu, cpu_perf_mask))
		return remove_input_boost_freq_perf;
	return remove_input_boost_freq_gold;
}

static void update_online_cpu_policy(void)
{
	unsigned int cpu;
	/* Only one CPU from each cluster needs to be updated */
	get_online_cpus();
	cpufreq_update_policy(0);
	cpufreq_update_policy(4);
	cpufreq_update_policy(7);
	put_online_cpus();
}
	
static void update_stune_boost(struct boost_drv *b) {
   	bool boost = false;
	unsigned int stune_boost_level = default_level_stune_boost;

	if (unlikely(!test_bit(SCREEN_ON, &b->cpu_state))) {
		set_stune_boost("top-app", sleep_level_stune_boost);
		return;
	}
	
	if (test_bit(FLEX_STUNE_BOOST, &b->stune_state)) {
		stune_boost_level = base_stune_boost+flex_stune_boost_offset;
		boost = true;
	}

	if (test_bit(INPUT_STUNE_BOOST, &b->stune_state)) {
		stune_boost_level = base_stune_boost+input_stune_boost_offset;
		boost = true;
	}

	if (test_bit(MAX_STUNE_BOOST, &b->stune_state)) {
		stune_boost_level = base_stune_boost+max_stune_boost_offset;
		boost = true;
	}
	
	if (boost) {
		set_stune_boost("top-app", stune_boost_level);
	} else
		set_stune_boost("top-app", default_level_stune_boost);
}

static void set_gpu_boost(struct boost_drv *b, int freq)
{
	int level;
	if (freq == gpu_prev_freq)
		return;
	else 
		gpu_prev_freq = freq;

	if (likely(gpu_oc)) {
		if (freq == 427)
			level = 5;
		if (freq == 345)
			level = 6;
		if (freq == 257)
			level = 7;
		if (freq == 180)
			level = 8;
	} else {
		if (freq == 427)
			level = 3;
		if (freq == 345)
			level = 4;
		if (freq == 257)
			level = 5;
	}

	mutex_lock(&b->gpu_device->mutex);
	b->gpu_pwr->min_pwrlevel = level;
	mutex_unlock(&b->gpu_device->mutex);
}

static void update_gpu_boost(struct boost_drv *b) {
	if (unlikely(!test_bit(SCREEN_ON, &b->cpu_state))) {
		set_gpu_boost(b, gpu_sleep_freq);
		return;
	}

	if (test_bit(GPU_INPUT_BOOST, &b->gpu_state) || test_bit(GPU_FLEX_BOOST, &b->gpu_state)) {
		set_gpu_boost(b, gpu_boost_freq);
	} else {
		set_gpu_boost(b, gpu_min_freq);
	}
}

static void __cpu_input_boost_kick(struct boost_drv *b)
{
	if (input_boost_duration == 0)
		return;
	if (!mod_delayed_work(b->wq_i, &b->input_unboost,
			msecs_to_jiffies(input_boost_duration))) {
		if (!test_bit(INPUT_BOOST, &b->cpu_state)) {
			set_bit(INPUT_BOOST, &b->cpu_state);
			if (!smumode)
				wake_up(&b->cpu_boost_waitq);
		}
	}
	if (!mod_delayed_work(b->wq_istu, &b->input_stune_unboost,
			msecs_to_jiffies(input_boost_duration+stune_boost_extender_ms))) {
		if (!test_bit(INPUT_STUNE_BOOST, &b->cpu_state)) {
			set_bit(INPUT_STUNE_BOOST, &b->stune_state);	
			wake_up(&b->stune_boost_waitq);
		}
	}	
	if (!mod_delayed_work(b->wq_gpu, &b->gpu_unboost,
			msecs_to_jiffies(input_boost_duration+gpu_boost_extender_ms))) {
		if (!test_bit(GPU_INPUT_BOOST, &b->cpu_state)) {
			set_bit(GPU_INPUT_BOOST, &b->gpu_state);
			wake_up(&b->gpu_boost_waitq);
		}
	}
}

static void __cpu_input_boost_kick_core(struct boost_drv *b,
				       unsigned int duration_ms, unsigned int cpu)
{
	b->cpu = cpu;
	if (!mod_delayed_work(b->wq_core, &b->core_unboost,
			      msecs_to_jiffies(duration_ms))) {
		if (!test_bit(CORE_BOOST, &b->cpu_state)) {
			set_bit(CORE_BOOST, &b->cpu_state);
			if (!smumode)
				wake_up(&b->cpu_boost_waitq);
		}
	}	
}

void cpu_input_boost_kick_core(unsigned int duration_ms, unsigned int cpu)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (duration_ms == 0)
		return;

	if (!test_bit(SCREEN_ON, &b->cpu_state))
		return;

	__cpu_input_boost_kick_core(b, duration_ms, cpu);
}

static void __cpu_input_boost_kick_ufs(struct boost_drv *b,
				       unsigned int duration_ms)
{
	if (!mod_delayed_work(b->wq_ufs, &b->ufs_unboost,
			msecs_to_jiffies(duration_ms))) {
		if (!test_bit(UFS_BOOST, &b->cpu_state)) {
			set_bit(UFS_BOOST, &b->cpu_state);
			wake_up(&b->cpu_boost_waitq);
		}
	}
}

static void __cpu_input_boost_kick_cluster1(struct boost_drv *b,
				       unsigned int duration_ms)
{
	if (!mod_delayed_work(b->wq_cl1, &b->cluster1_unboost,
			msecs_to_jiffies(duration_ms))) {
		if (!test_bit(CLUSTER1_BOOST, &b->cpu_state)) {
			set_bit(CLUSTER1_BOOST, &b->cpu_state);
			if (!smumode)
				wake_up(&b->cpu_boost_waitq);
		}
	}
	if (!mod_delayed_work(b->wq_mstu, &b->max_stune_unboost,
			msecs_to_jiffies(duration_ms+max_stune_boost_extender_ms))) {
		if (!test_bit(MAX_STUNE_BOOST, &b->cpu_state)) {
			set_bit(MAX_STUNE_BOOST, &b->stune_state);
			wake_up(&b->stune_boost_waitq);
		}
	}
}

static void __cpu_input_boost_kick_cluster2(struct boost_drv *b,
				       unsigned int duration_ms)
{
	if (!mod_delayed_work(b->wq_cl2, &b->cluster2_unboost,
			msecs_to_jiffies(duration_ms))) {
		if (!test_bit(CLUSTER2_BOOST, &b->cpu_state)) {
			set_bit(CLUSTER2_BOOST, &b->cpu_state);
			if (!smumode)
				wake_up(&b->cpu_boost_waitq);
		}
	}
	if (!test_bit(CLUSTER1_BOOST, &b->cpu_state) || !test_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state))
		if (!mod_delayed_work(b->wq_mstu, &b->max_stune_unboost,
				msecs_to_jiffies(duration_ms+max_stune_boost_extender_ms))) {
			if (!test_bit(MAX_STUNE_BOOST, &b->cpu_state)) {
				set_bit(MAX_STUNE_BOOST, &b->stune_state);
				wake_up(&b->stune_boost_waitq);
			}
		}
}

void cpu_input_boost_kick_ufs(unsigned int duration_ms)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (duration_ms == 0)
		return;

	if (!test_bit(SCREEN_ON, &b->cpu_state))
		return;

	__cpu_input_boost_kick_ufs(b, duration_ms);
}

void cpu_input_boost_kick_cluster1(unsigned int duration_ms)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (duration_ms == 0)
		return;

	if (!test_bit(SCREEN_ON, &b->cpu_state))
		return;

	__cpu_input_boost_kick_cluster1(b, duration_ms);
}

void cpu_input_boost_kick_cluster2(unsigned int duration_ms)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (little_only || duration_ms == 0)
		return;

	if (!test_bit(SCREEN_ON, &b->cpu_state))
		return;

	__cpu_input_boost_kick_cluster2(b, duration_ms);
}

static void __cpu_input_boost_kick_cluster1_wake(struct boost_drv *b,
				       unsigned int duration_ms)
{
	if (!mod_delayed_work(b->wq_cl1, &b->cluster1_unboost,
			msecs_to_jiffies(duration_ms))) {
		set_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state);
		if (!smumode)
			wake_up(&b->cpu_boost_waitq);
	}
	if (!mod_delayed_work(b->wq_mstu, &b->max_stune_unboost,
			msecs_to_jiffies(duration_ms+max_stune_boost_extender_ms))) {
		set_bit(MAX_STUNE_BOOST, &b->stune_state);
		wake_up(&b->stune_boost_waitq);
	}
}

static void __cpu_input_boost_kick_cluster2_wake(struct boost_drv *b,
				       unsigned int duration_ms)
{
	if (!mod_delayed_work(b->wq_cl2, &b->cluster2_unboost,
			msecs_to_jiffies(duration_ms))) {
		set_bit(CLUSTER2_WAKE_BOOST, &b->cpu_state);
		if (!smumode)
			wake_up(&b->cpu_boost_waitq);
	}
	if (!test_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state) || !test_bit(CLUSTER1_BOOST, &b->cpu_state))
		if (!mod_delayed_work(b->wq_mstu, &b->max_stune_unboost,
				msecs_to_jiffies(duration_ms+max_stune_boost_extender_ms))) {
			set_bit(MAX_STUNE_BOOST, &b->stune_state);
			wake_up(&b->stune_boost_waitq);
		}
}

void cpu_input_boost_kick_cluster1_wake(unsigned int duration_ms)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (duration_ms == 0)
		return;

	__cpu_input_boost_kick_cluster1_wake(b, duration_ms);
}

void cpu_input_boost_kick_cluster2_wake(unsigned int duration_ms)
{
	unsigned long boost_jiffies = msecs_to_jiffies(duration_ms);
	struct boost_drv *b = &boost_drv_g;

	if (duration_ms == 0)
		return;

	__cpu_input_boost_kick_cluster2_wake(b, duration_ms);
}

static void __gpu_boost_flex(struct boost_drv *b, unsigned int duration_ms)
{
	unsigned int act_duration_ms = flex_boost_duration;

	if (duration_ms > 0)
		act_duration_ms = duration_ms;

	if (!mod_delayed_work(b->wq_gpu_flex, &b->gpu_flex_unboost,
			msecs_to_jiffies(act_duration_ms))) {
		if (!test_bit(GPU_FLEX_BOOST, &b->cpu_state)) {
			set_bit(GPU_FLEX_BOOST, &b->gpu_state);
			wake_up(&b->gpu_boost_waitq);
		}
	}
}

void gpu_boost_flex(unsigned int duration_ms)
{
	struct boost_drv *b = &boost_drv_g;
	
	if (!test_bit(SCREEN_ON, &b->cpu_state) || ((flex_boost_duration == 0) && (duration_ms == 0)))
		return;

	__gpu_boost_flex (b, duration_ms);
}

static void __cpu_input_boost_kick_flex(struct boost_drv *b, unsigned int duration_ms)
{
	unsigned int act_duration_ms = flex_boost_duration;

	if (duration_ms > 0)
		act_duration_ms = duration_ms;

	if (!mod_delayed_work(b->wq_f, &b->flex_unboost,
			msecs_to_jiffies(act_duration_ms))) {
		if (!test_bit(FLEX_BOOST, &b->cpu_state)) {
			set_bit(FLEX_BOOST, &b->cpu_state);
			if (!smumode)
				wake_up(&b->cpu_boost_waitq);
		}
	}
	if (base_stune_boost+flex_stune_boost_offset > 0)
		if (!mod_delayed_work(b->wq_fstu, &b->flex_stune_unboost,
			msecs_to_jiffies(act_duration_ms))) {
		if (!test_bit(FLEX_STUNE_BOOST, &b->cpu_state)) {
			set_bit(FLEX_STUNE_BOOST, &b->stune_state);
			wake_up(&b->stune_boost_waitq);
		}
	}
}

void cpu_input_boost_kick_flex(unsigned int duration_ms)
{
	struct boost_drv *b = &boost_drv_g;
	
	if (!test_bit(SCREEN_ON, &b->cpu_state) || ((flex_boost_duration == 0) && (duration_ms == 0)))
		return;

	__cpu_input_boost_kick_flex(b, duration_ms);
}

static void input_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), input_unboost);
	
	clear_bit(INPUT_BOOST, &b->cpu_state);
	if (!smumode)
		wake_up(&b->cpu_boost_waitq);
}

static void core_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), core_unboost);

	clear_bit(CORE_BOOST, &b->cpu_state);
	if (!smumode)
		wake_up(&b->cpu_boost_waitq);
}

static void ufs_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), ufs_unboost);

	clear_bit(UFS_BOOST, &b->cpu_state);
	wake_up(&b->cpu_boost_waitq);
}

static void cluster1_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), cluster1_unboost);

	clear_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state);
	clear_bit(CLUSTER1_BOOST, &b->cpu_state);
	if (!smumode)
		wake_up(&b->cpu_boost_waitq);
}

static void cluster2_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), cluster2_unboost);
	
	clear_bit(CLUSTER2_WAKE_BOOST, &b->cpu_state);
	clear_bit(CLUSTER2_BOOST, &b->cpu_state);
	if (!smumode)
		wake_up(&b->cpu_boost_waitq);
}

static void flex_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), flex_unboost);

	clear_bit(FLEX_BOOST, &b->cpu_state);
	if (!smumode)
		wake_up(&b->cpu_boost_waitq);
}

static void input_stune_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), input_stune_unboost);

	clear_bit(INPUT_STUNE_BOOST, &b->stune_state);
	wake_up(&b->stune_boost_waitq);
}

static void max_stune_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), max_stune_unboost);

	clear_bit(MAX_STUNE_BOOST, &b->stune_state);
	wake_up(&b->stune_boost_waitq);
}

static void flex_stune_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), flex_stune_unboost);

	clear_bit(FLEX_STUNE_BOOST, &b->stune_state);
	wake_up(&b->stune_boost_waitq);
}

static void gpu_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), gpu_unboost);

	clear_bit(GPU_INPUT_BOOST, &b->gpu_state);
	wake_up(&b->gpu_boost_waitq);
}

static void gpu_flex_unboost_worker(struct work_struct *work)
{
	struct boost_drv *b = container_of(to_delayed_work(work),
					   typeof(*b), gpu_flex_unboost);

	clear_bit(GPU_FLEX_BOOST, &b->gpu_state);
	wake_up(&b->gpu_boost_waitq);
}

static void cancel_all_wq (struct boost_drv *b)
{
	cancel_delayed_work(&b->input_unboost);
	cancel_delayed_work(&b->core_unboost);
	cancel_delayed_work(&b->cluster1_unboost);
	cancel_delayed_work(&b->cluster2_unboost);
	cancel_delayed_work(&b->flex_unboost);
	cancel_delayed_work(&b->input_stune_unboost);
	cancel_delayed_work(&b->max_stune_unboost);
	cancel_delayed_work(&b->flex_stune_unboost);
	cancel_delayed_work(&b->gpu_unboost);
	cancel_delayed_work(&b->gpu_flex_unboost);
}

update_ufs_boost(struct boost_drv *b) {
	if (ufs_boost) {
		if (test_bit(UFS_BOOST, &b->cpu_state)) {
			//set_ufshcd_hibern8_enable_status(0);
			set_ufshcd_clkgate_enable_status(0);
		} else {
			//set_ufshcd_hibern8_enable_status(1);
			set_ufshcd_clkgate_enable_status(1);
		}
	}
}

static int cpu_boost_thread(void *data)
{
	static struct sched_param sched_max_rt_prio;
	struct boost_drv *b = data;
	unsigned long old_state = 0;

	if (input_thread_prio == 99)
 		sched_max_rt_prio.sched_priority = MAX_RT_PRIO - 1;
	else 
		sched_max_rt_prio.sched_priority = input_thread_prio;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sched_max_rt_prio);

	while (!kthread_should_stop()) {
		unsigned long curr_state;

		wait_event(b->cpu_boost_waitq,
			(curr_state = READ_ONCE(b->cpu_state)) != old_state ||
			kthread_should_stop());
		old_state = curr_state;
		if (!smumode)
			update_online_cpu_policy();
		update_ufs_boost(b);
	}
	return 0;
}

static int gpu_boost_thread(void *data)
{
	static struct sched_param sched_max_rt_prio;
	struct boost_drv *b = data;
	unsigned long old_state = 0;

	if (input_thread_prio == 99)
 		sched_max_rt_prio.sched_priority = MAX_RT_PRIO - 1;
	else 
		sched_max_rt_prio.sched_priority = input_thread_prio;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sched_max_rt_prio);

	while (!kthread_should_stop()) {
		unsigned long curr_state;

		wait_event(b->gpu_boost_waitq,
			(curr_state = READ_ONCE(b->gpu_state)) != old_state ||
			kthread_should_stop());
		old_state = curr_state;
		update_gpu_boost(b);
	}
	return 0;
}

static int stune_boost_thread(void *data)
{
	static struct sched_param sched_max_rt_prio;
	struct boost_drv *b = data;
	unsigned long old_state = 0;

	if (input_thread_prio == 99)
 		sched_max_rt_prio.sched_priority = MAX_RT_PRIO - 1;
	else 
		sched_max_rt_prio.sched_priority = input_thread_prio;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sched_max_rt_prio);

	while (!kthread_should_stop()) {
		unsigned long curr_state;

		wait_event(b->stune_boost_waitq,
			(curr_state = READ_ONCE(b->stune_state)) != old_state ||
			kthread_should_stop());
		old_state = curr_state;
		update_stune_boost(b);
	}
	return 0;
}

static int cpu_notifier_cb(struct notifier_block *nb, unsigned long action,
			   void *data)
{
	struct boost_drv *b = container_of(nb, typeof(*b), cpu_notif);
	struct cpufreq_policy *policy = data;
	unsigned int min_freq;

	if (action != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	if (strcmp(policy->governor->name, "smurfutil"))
		smumode = true;
	else 
		smumode = false;

	if (smumode) {
		if (!(0x01 & b->cpu_state)) {
			if (policy->cpu < 4) {
				policy->min=sleep_freq_lp;
			}
			if ((policy->cpu > 3) && (policy->cpu < 7)) {
				policy->min=sleep_freq_hp;
			}
			if (policy->cpu==7) {
				policy->min=sleep_freq_gold;
			}
			return NOTIFY_OK;
		}
		if (0x01 & b->cpu_state) {
			if (policy->cpu < 4) {
				policy->min=get_min_freq(policy);
			}
			if ((policy->cpu > 3) && (policy->cpu < 7)) {
				policy->min=get_min_freq(policy);
			}
			if (policy->cpu==7) {
				policy->min=get_min_freq(policy);
			}
			return NOTIFY_OK;
		}
	}

	if (!smumode) {
		if (test_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state) && (policy->cpu < 4)) {
			policy->min = get_max_boost_freq(policy, 0);
			return NOTIFY_OK;
		}

		if (test_bit(CLUSTER2_WAKE_BOOST, &b->cpu_state)) {
			if ((policy->cpu > 3) && (policy->cpu < 7))
				policy->min = get_max_boost_freq(policy, 0);
			if ((policy->cpu  == 7) && boost_gold) 
				policy->min = get_max_boost_freq(policy, 0);
			return NOTIFY_OK;
		}

		if (!test_bit(SCREEN_ON, &b->cpu_state)) {
			if (policy->cpu < 4) {
				policy->min=sleep_freq_lp;
			}
			if ((policy->cpu > 3) && (policy->cpu < 7)) {
				policy->min=sleep_freq_hp;
			}
			if (policy->cpu==7) {
				policy->min=sleep_freq_gold;
			}
			return NOTIFY_OK;
		}

		if (test_bit(CORE_BOOST, &b->cpu_state) && (policy->cpu == b->cpu)) {
			b->cpu = 8;
			policy->min = policy->max;
			return NOTIFY_OK;
		}

		/* Boost CPU to max frequency for max boost */
		if (test_bit(CLUSTER1_BOOST, &b->cpu_state) && (policy->cpu < 4)) {
			policy->min = get_max_boost_freq(policy, 0);
			return NOTIFY_OK;
		}
		if (test_bit(CLUSTER2_BOOST, &b->cpu_state)) {
			if ((policy->cpu > 3) && (policy->cpu < 7))
				policy->min = get_max_boost_freq(policy, 0);
			if ((policy->cpu  == 7) && boost_gold)
				policy->min = get_max_boost_freq(policy, 0);
			return NOTIFY_OK;
		}

		if (test_bit(INPUT_BOOST, &b->cpu_state) || test_bit(FLEX_BOOST, &b->cpu_state)) {
			if (test_bit(INPUT_BOOST, &b->cpu_state)) {
				if (policy->cpu < 4)
					policy->min = get_input_boost_freq(policy);
				if ((policy->cpu > 3) && (policy->cpu < 7))
					policy->min = get_input_boost_freq(policy);
				if ((policy->cpu  == 7) && boost_gold)
					policy->min = get_input_boost_freq(policy);
				return NOTIFY_OK;
			}
			if (test_bit(FLEX_BOOST, &b->cpu_state)) {
				if (policy->cpu < 4)
					policy->min = get_flex_boost_freq(policy, 0);
				if ((policy->cpu > 3) && (policy->cpu < 7))
					policy->min = get_flex_boost_freq(policy, 0);
				if ((policy->cpu  == 7) && boost_gold)
					policy->min = get_flex_boost_freq(policy, 0);
				return NOTIFY_OK;
			}
		}
		policy->min = get_min_freq(policy);
		return NOTIFY_OK;
	}
}

#ifdef CONFIG_DRM_MSM
static int msm_drm_notifier_cb(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct boost_drv *b = container_of(nb, typeof(*b), msm_drm_notif);
	struct msm_drm_notifier *evdata = data;
	int *blank = evdata->data;

	/* Parse framebuffer blank events as soon as they occur */
	if (action != MSM_DRM_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	/* Boost when the screen turns on and unboost when it turns off */
	if (*blank == MSM_DRM_BLANK_UNBLANK_CUST) {
		set_bit(SCREEN_ON, &b->cpu_state);
		update_online_cpu_policy();	
		cpu_input_boost_kick_cluster1(1000);
		cpu_input_boost_kick_cluster2(1000);
	} else if (*blank == MSM_DRM_BLANK_POWERDOWN_CUST) {
		if (test_bit(SCREEN_ON, &b->cpu_state)) {
			clear_bit(SCREEN_ON, &b->cpu_state);
			clear_bit(INPUT_BOOST, &b->cpu_state);
			clear_bit(FLEX_BOOST, &b->cpu_state);
			clear_bit(CORE_BOOST, &b->cpu_state);
			clear_bit(CLUSTER1_BOOST, &b->cpu_state);
			clear_bit(CLUSTER2_BOOST, &b->cpu_state);
			clear_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state);
			clear_bit(CLUSTER2_WAKE_BOOST, &b->cpu_state);
			clear_bit(INPUT_STUNE_BOOST, &b->stune_state);
			clear_bit(MAX_STUNE_BOOST, &b->stune_state);
			clear_bit(FLEX_STUNE_BOOST, &b->stune_state);
			clear_bit(GPU_INPUT_BOOST, &b->gpu_state);
			cancel_all_wq (b);
			set_gpu_boost(b, gpu_sleep_freq);
			set_stune_boost("top-app", default_level_stune_boost);
			update_online_cpu_policy();
			pr_info("Screen off, boosts turned off\n");
			pr_info("Screen off, GPU frequency sleep\n");
			pr_info("Screen off, CPU frequency sleep\n");
		}
	}
	return NOTIFY_OK;
}
#else
static int fb_notifier_cb(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct boost_drv *b = container_of(nb, typeof(*b), fb_notif);
	int *blank = ((struct fb_event *)data)->data;

	/* Parse framebuffer blank events as soon as they occur */
	if (action != FB_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	/* Boost when the screen turns on and unboost when it turns off */
	if (*blank == FB_BLANK_UNBLANK) {
		cpu_input_boost_kick_cluster1_wake(1000);
		cpu_input_boost_kick_cluster2_wake(1000);	
		set_bit(SCREEN_ON, &b->cpu_state);
	} else {
		if (test_bit(SCREEN_ON, &b->cpu_state)) {
			clear_bit(SCREEN_ON, &b->cpu_state);
			clear_bit(INPUT_BOOST, &b->cpu_state);
			clear_bit(FLEX_BOOST, &b->cpu_state);
			clear_bit(CORE_BOOST, &b->cpu_state);
			clear_bit(CLUSTER1_BOOST, &b->cpu_state);
			clear_bit(CLUSTER2_BOOST, &b->cpu_state);
			clear_bit(CLUSTER1_WAKE_BOOST, &b->cpu_state);
			clear_bit(CLUSTER2_WAKE_BOOST, &b->cpu_state);
			clear_bit(INPUT_STUNE_BOOST, &b->stune_state);
			clear_bit(MAX_STUNE_BOOST, &b->stune_state);
			clear_bit(FLEX_STUNE_BOOST, &b->stune_state);
			clear_bit(GPU_INPUT_BOOST, &b->gpu_state);
			cancel_all_wq (b);
			set_gpu_boost(b, gpu_sleep_freq);
			set_stune_boost("top-app", default_level_stune_boost);
			update_online_cpu_policy();
			pr_info("Screen off, boosts turned off\n");
			pr_info("Screen off, GPU frequency sleep\n");
			pr_info("Screen off, CPU frequency sleep\n");
		}
	}
	return NOTIFY_OK;
}
#endif

static void cpu_input_boost_input_event(struct input_handle *handle,
					unsigned int type, unsigned int code,
					int value)
{
	struct boost_drv *b = handle->handler->private;

	__cpu_input_boost_kick(b);
}

static int cpu_input_boost_input_connect(struct input_handler *handler,
					 struct input_dev *dev,
					 const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpu_input_boost_handle";

	ret = input_register_handle(handle);
	if (ret)
		goto free_handle;

	ret = input_open_device(handle);
	if (ret)
		goto unregister_handle;

	return 0;

unregister_handle:
	input_unregister_handle(handle);
free_handle:
	kfree(handle);
	return ret;
}

static void cpu_input_boost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id cpu_input_boost_ids[] = {
	/* Multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) }
	},
	/* Touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) }
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) }
	},
	{ }
};

static struct input_handler cpu_input_boost_input_handler = {
	.event		= cpu_input_boost_input_event,
	.connect	= cpu_input_boost_input_connect,
	.disconnect	= cpu_input_boost_input_disconnect,
	.name		= "cpu_input_boost_handler",
	.id_table	= cpu_input_boost_ids
};

static int __init cpu_input_boost_init(void)
{
	struct task_struct *boost_thread;
	struct task_struct *boost_thread_gpu;
	struct task_struct *boost_thread_stune;
	struct boost_drv *b = &boost_drv_g;
	gov_cpu_state = &boost_drv_g;
	int ret;
	
	b->cpu_state = 0;
	b->gpu_state = 0;
	b->stune_state = 0;
	set_stune_boost("top-app", sleep_level_stune_boost);
	
	b->wq_i = alloc_workqueue("cpu_input_boost_wq_i", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_i) {
		ret = -ENOMEM;
		return ret;
	}
	
	b->wq_f = alloc_workqueue("cpu_input_boost_wq_f", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_f) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_ufs = alloc_workqueue("cpu_input_boost_wq_ufs", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_ufs) {
		ret = -ENOMEM;
		return ret;
	}
	
	b->wq_cl1 = alloc_workqueue("cpu_input_boost_wq_cl1", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_cl1) {
		ret = -ENOMEM;
		return ret;
	}
	
	b->wq_cl2 = alloc_workqueue("cpu_input_boost_wq_cl2", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_cl2) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_istu = alloc_workqueue("cpu_input_boost_wq_istu", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_istu) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_mstu = alloc_workqueue("cpu_input_boost_wq_mstu", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_mstu) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_gpu = alloc_workqueue("cpu_input_boost_wq_gpu", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_gpu) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_gpu_flex = alloc_workqueue("cpu_input_boost_wq_gpu_flex", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_gpu_flex) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_fstu = alloc_workqueue("cpu_input_boost_wq_fstu", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_fstu) {
		ret = -ENOMEM;
		return ret;
	}

	b->wq_core = alloc_workqueue("cpu_input_boost_wq_core", WQ_POWER_EFFICIENT, 0);
	if (!b->wq_core) {
		ret = -ENOMEM;
		return ret;
	}

	set_bit(SCREEN_ON, &b->cpu_state);

	b->cpu_notif.notifier_call = cpu_notifier_cb;
	b->cpu_notif.priority = INT_MAX;
	ret = cpufreq_register_notifier(&b->cpu_notif, CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		pr_err("Failed to register cpufreq notifier, err: %d\n", ret);
		return ret;
	}

	cpu_input_boost_input_handler.private = b;
	ret = input_register_handler(&cpu_input_boost_input_handler);
	if (ret) {
		pr_err("Failed to register input handler, err: %d\n", ret);
		goto unregister_cpu_notif;
	}
#ifdef CONFIG_DRM_MSM
	b->msm_drm_notif.notifier_call = msm_drm_notifier_cb;
	b->msm_drm_notif.priority = INT_MAX-2;
	ret = msm_drm_register_client(&b->msm_drm_notif);
	if (ret) {
		pr_err("Failed to register msm_drm notifier, err: %d\n", ret);
		goto unregister_handler;
	}
#else
	b->fb_notif.notifier_call = fb_notifier_cb;
	b->fb_notif.priority = INT_MAX-2;
	ret = fb_register_client(&b->fb_notif);
	if (ret) {
		pr_err("Failed to register fb notifier, err: %d\n", ret);
		goto unregister_handler;
	}
#endif

	b->gpu_device = kgsl_get_device(KGSL_DEVICE_3D0);
	if (IS_ERR_OR_NULL(b->gpu_device))
		return 0;
	b->gpu_pwr = &b->gpu_device->pwrctrl;

	boost_thread = kthread_run_low_power(cpu_boost_thread, b, "cpu_boostd");
	if (IS_ERR(boost_thread)) {
		pr_err("Failed to start CPU boost thread, err: %ld\n",
		       PTR_ERR(boost_thread));
		goto unregister_drm_notif;
	}
	
	boost_thread_gpu = kthread_run_low_power(gpu_boost_thread, b, "gpu_boostd");
	if (IS_ERR(boost_thread_gpu)) {
		pr_err("Failed to start GPU boost thread, err: %ld\n",
		       PTR_ERR(boost_thread_gpu));
		goto unregister_drm_notif;
	}
	
	boost_thread_stune = kthread_run_low_power(stune_boost_thread, b, "stune_boostd");
	if (IS_ERR(boost_thread_stune)) {
		pr_err("Failed to start STUNE boost thread, err: %ld\n",
		       PTR_ERR(boost_thread_stune));
		goto unregister_drm_notif;
	}
	

	return 0;

unregister_drm_notif:
#ifdef CONFIG_DRM_MSM
	msm_drm_unregister_client(&b->msm_drm_notif);
#else
	fb_unregister_client(&b->fb_notif);
#endif
unregister_handler:
	input_unregister_handler(&cpu_input_boost_input_handler);
unregister_cpu_notif:
	cpufreq_unregister_notifier(&b->cpu_notif, CPUFREQ_POLICY_NOTIFIER);
	return ret;
}
late_initcall(cpu_input_boost_init);

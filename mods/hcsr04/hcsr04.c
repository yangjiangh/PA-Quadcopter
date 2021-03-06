/*
 * hcsr04.c
 *
 *  Created on: Sep 9, 2016
 *
 *  四轴飞行控制器  Copyright (C) 2016  李德强
 */

#include <hcsr04.h>

s32 st = 0;
s32 r  = 0;

pthread_t pthd;
s_engine* e = NULL;
s_params* p = NULL;

s_dis dis;

//信号噪声
f32 h_est_devi     = 0.01;
f32 h_measure_devi = 0.05;
//卡尔曼滤波
f32 h_est = 0.0, h_devi = 0.0;

//初始化陀螺仪
s32 __init(s_engine* engine, s_params* params)
{
	e  = engine;
	p  = params;
	st = 1;
	r  = 1;

	//设置引脚为输出引脚
	pinMode(PORT_CS_TRIG, OUTPUT);
	pinMode(PORT_CS_ECHO, INPUT);

	wiringPiISR(PORT_CS_ECHO, INT_EDGE_BOTH, &distance);

	pthread_create(&pthd, (const pthread_attr_t*)NULL, (void* (*)(void*)) & distance_trig, NULL);

	return 0;
}

s32 __destory(s_engine* e, s_params* p)
{
	r = 0;

	return 0;
}

s32 __status() { return st; }

void distance_trig()
{
	while (r)
	{
		//初始化HC-SR04
		digitalWrite(PORT_CS_TRIG, HIGH);
		//持续15微秒
		usleep(15);
		digitalWrite(PORT_CS_TRIG, LOW);
		usleep(50 * 1000);
	}
	st = 0;
}

void distance()
{
	//读取电平信号
	s32 value = digitalRead(PORT_CS_ECHO);
	//如果是高电平
	if (value)
	{
		//计时开始
		gettimeofday(&dis.timer_start, NULL);
		return;
	}

	//如果是低电平,计时结束
	gettimeofday(&dis.timer_end, NULL);
	//计算高电平时长
	long timer = (dis.timer_end.tv_sec - dis.timer_start.tv_sec) * 1000000 + (dis.timer_end.tv_usec - dis.timer_start.tv_usec);

	f32 height = 0;
	//如果超过30ms超出测距范围，有效范围4.0米
	if (timer > 30 * 1000)
	{
		//默认最远距离为5米
		height = 5.0;
	}
	else
	{
		//声音在空气中传播速度3.40m/s，距离往返除以2。
		height = (timer / 1000000.0) * 340.0 / 2.0;
	}
	//对高度卡尔曼滤波
	h_est = kalman_filter(h_est, h_est_devi, height, h_measure_devi, &h_devi);

	//设定引擎中调试值
	e->height = h_est * cos(e->tx) * cos(e->ty);
}

/***
 * est预估值
 * est_devi预估偏差
 * measure测量读数
 * measure_devi测量噪声
 * devi上一次最优偏差
 */
f32 kalman_filter(f32 est, f32 est_devi, f32 measure, f32 measure_devi, float* devi)
{
	//预估高斯噪声的偏差
	f32 q = sqrt((*devi) * (*devi) + est_devi * est_devi);
	//卡尔曼增益
	f32 kg = q * q / (q * q + measure_devi * measure_devi);
	//滤波结果
	f32 val = est + kg * (measure - est);
	//最优偏差
	*devi = sqrt((1.0 - kg) * q * q);

	return val;
}

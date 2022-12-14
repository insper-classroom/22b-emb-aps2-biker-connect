/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ASF/thirdparty/lvgl8/src/font/lv_font.h"
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"
#include "background.h"
#include "wheel.h"
#include "string.h"
#include "arm_math.h"

#define SENSOR_PIO PIOA
#define SENSOR_PIO_ID ID_PIOA
#define SENSOR_IDX 19
#define SENSOR_IDX_MASK (1u << SENSOR_IDX)

#define VERMELHO_PIO PIOD
#define VERMELHO_PIO_ID ID_PIOD
#define VERMELHO_IDX 20
#define VERMELHO_IDX_MASK (1u << VERMELHO_IDX)

#define VERDE_PIO PIOD
#define VERDE_PIO_ID ID_PIOD
#define VERDE_IDX 25
#define VERDE_IDX_MASK (1u << VERDE_IDX)

#define AZUL_PIO PIOB
#define AZUL_PIO_ID ID_PIOB
#define AZUL_IDX 0
#define AZUL_IDX_MASK (1u << AZUL_IDX)

#define RESET 0 
#define PAUSE 1
#define PLAY 2
#define FATOR_CONVERSAO 0.921f  //Compensa atraso do RTOS
#define DELTA 0.001f

SemaphoreHandle_t xMutexLVGL;
SemaphoreHandle_t xSemaphoreScreen1;
SemaphoreHandle_t xSemaphoreScreen2;
SemaphoreHandle_t xSemaphoreScreen3;
SemaphoreHandle_t xSemaphoreRTC;
QueueHandle_t xQueueCronometro;
QueueHandle_t xQueuedt;
QueueHandle_t xQueueRaio;


typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t seccond;
} calendar;

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)


/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_CHANGE_STACK_SIZE                (1024/sizeof(portSTACK_TYPE))
#define TASK_CHANGE_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_UPDATE_STACK_SIZE                (1024/sizeof(portSTACK_TYPE))
#define TASK_UPDATE_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_SPEED_STACK_SIZE                (1024/sizeof(portSTACK_TYPE))
#define TASK_SPEED_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_SIMULATOR_STACK_SIZE (1024 / sizeof(portSTACK_TYPE))
#define TASK_SIMULATOR_STACK_PRIORITY (tskIDLE_PRIORITY)

#define RAIO 0.508/2
#define VEL_MAX_KMH  5.0f
#define VEL_MIN_KMH  1.0f
#define RAMP 

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* Handlers and callbacks                                               */
/************************************************************************/
void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		// o código para irq de alame vem aqui
	}
	
	/* seccond tick */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		xSemaphoreGiveFromISR(xSemaphoreRTC, &xHigherPriorityTaskWoken);
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void sensor_callback(void){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	int dt = rtt_read_timer_value(RTT);
	printf("rtt %d \n", dt);
	xQueueSendFromISR(xQueuedt,&dt,&xHigherPriorityTaskWoken);
	RTT_init(1000,0,NULL);
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/
LV_FONT_DECLARE(lv_font_montserrat_48);
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_IMG_DECLARE(logo);
LV_IMG_DECLARE(today);
LV_IMG_DECLARE(vmedia);
LV_IMG_DECLARE(button2);
LV_IMG_DECLARE(average);
LV_IMG_DECLARE(distance);
LV_IMG_DECLARE(clock_sig);

static lv_obj_t * scr1;  // screen 1
static lv_obj_t * scr2;  // screen 2
static lv_obj_t * scr3;  // screen 2

static lv_obj_t * labelBtn1;
static lv_obj_t * labelBtn2;
static lv_obj_t * labelBtn3;
static lv_obj_t * labelBtn4;
static lv_obj_t * labelBtn5;

lv_obj_t * inst_speed;
lv_obj_t * actual_distance;
lv_obj_t * clock_screen1;
lv_obj_t * clock_screen2;
lv_obj_t * clock_screen3;
lv_obj_t * time_cron;
lv_obj_t * acce_indication;
lv_obj_t * distance_cron;
lv_obj_t * vel_cron;
lv_obj_t * bike_wheel;

static void homescreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen1);
	}
}

static void routescreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen2);
	}
}

static void configscreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen3);
	}
}

static void playpause_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	static state = 0;
	if(code == LV_EVENT_CLICKED) {
		if (state){
			lv_label_set_text_fmt(labelBtn4, LV_SYMBOL_PLAY);
			int rote_status = PAUSE;
			xQueueSend(xQueueCronometro,&rote_status,0);
		}
		else{
			lv_label_set_text_fmt(labelBtn4, LV_SYMBOL_PAUSE);
			int rote_status = PLAY;
			xQueueSend(xQueueCronometro,&rote_status,0);
		}
		 state = !state;
	}
}

static void stop_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		lv_label_set_text_fmt(labelBtn4, LV_SYMBOL_PLAY);
		int rote_status = RESET;
		xQueueSend(xQueueCronometro,&rote_status,0);
	}
}

static void roller_handler(lv_event_t * e)
{
	int zoom_bike;
	volatile float radius_size;
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	if(code == LV_EVENT_VALUE_CHANGED) {
		char buf[10];
		lv_roller_get_selected_str(obj, buf, sizeof(buf));
		if (buf[1]=='6'){
			radius_size = 0.4064 / 2;
			zoom_bike=  200;
		}
		if (buf[1]=='8'){
			radius_size = 0.4572 / 2;
			zoom_bike =  220;
		}
		if (buf[1]=='0' && buf[0]=='2'){
			radius_size = 0.508 / 2;
			zoom_bike =  240;
		}
		if (buf[1]=='4'){
			radius_size = 0.6096 / 2;
			zoom_bike= 260;
		}
		if (buf[1]=='6'){
			radius_size = 0.6604 / 2;
			zoom_bike= 280;
		}
		if (buf[1]=='7'){
			radius_size = 0.6985 / 2;
			zoom_bike= 300;
		}
		if (buf[1]=='0' && buf[0]=='7'){
			radius_size = 0.7366 / 2;
			zoom_bike= 320;
		}
		xQueueSend(xQueueRaio,&radius_size,0);
		lv_img_set_zoom(bike_wheel, zoom_bike);
	}
}


void lv_screen_1(lv_obj_t * screen) {
	// background
	lv_obj_t * background = lv_img_create(screen);
	lv_img_set_src(background, &white);
	lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
	
	// Top objects
	lv_obj_t * logo_img = lv_img_create(screen);
	lv_img_set_src(logo_img, &logo);
	lv_obj_align(logo_img, LV_ALIGN_TOP_LEFT, 0, 0);
	
	lv_obj_t * label_screen;
	label_screen = lv_label_create(screen);
	lv_obj_align(label_screen, LV_ALIGN_TOP_MID, 0 , 10);
	lv_obj_set_style_text_font(label_screen, &lv_font_montserrat_16, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(label_screen, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(label_screen,LV_SYMBOL_HOME " Home");
	
	clock_screen1 = lv_label_create(screen);
	lv_obj_align(clock_screen1, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock_screen1, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock_screen1, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock_screen1,"12:05:34");
	
	//Main topics of screen
	lv_obj_t * animimg0 = lv_animimg_create(screen);
	lv_obj_align(animimg0, LV_ALIGN_LEFT_MID, 30, -16);
	lv_animimg_set_src(animimg0, (lv_img_dsc_t **) anim_imgs, 116);
	lv_animimg_set_duration(animimg0, 1000);
	lv_animimg_set_repeat_count(animimg0, LV_ANIM_REPEAT_INFINITE);
	lv_animimg_start(animimg0);
	
	//Line points
	static lv_point_t line_points[] = { {5, 0}, {315, 0} };

	/*Create style of lines*/
	static lv_style_t style_line;
	lv_style_init(&style_line);
	lv_style_set_line_width(&style_line, 3);
	lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_CYAN));
	lv_style_set_line_rounded(&style_line, true);

	// Bottom Line
	lv_obj_t * bottom_line;
	bottom_line = lv_line_create(screen);
	lv_line_set_points(bottom_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(bottom_line, &style_line, 0);
	lv_obj_align(bottom_line, LV_ALIGN_BOTTOM_MID, 0, -60);
	
	// Top Line
	lv_obj_t * top_line;
	top_line = lv_line_create(screen);
	lv_line_set_points(top_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(top_line, &style_line, 0);
	lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 40);
	
	// Acceleration indication
	acce_indication = lv_label_create(screen);
	lv_obj_align(acce_indication, LV_ALIGN_RIGHT_MID, -130, -50);
	lv_obj_set_style_text_font(acce_indication, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(acce_indication, lv_color_make(0x00, 0xff, 0x00), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(acce_indication,LV_SYMBOL_UP);
	
	// Diary distance
	lv_obj_t * diary_distance = lv_img_create(screen);
	lv_img_set_src(diary_distance, &today);
	lv_obj_align(diary_distance, LV_ALIGN_RIGHT_MID, -110, 20);
	
	
	inst_speed = lv_label_create(screen);
	lv_obj_align(inst_speed, LV_ALIGN_RIGHT_MID, -55 , -50);
	lv_obj_set_style_text_font(inst_speed, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(inst_speed, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(inst_speed," %02d" , 0);
	
	actual_distance = lv_label_create(screen);
	lv_obj_align(actual_distance, LV_ALIGN_RIGHT_MID, -55 , 20);
	lv_obj_set_style_text_font(actual_distance, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(actual_distance, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(actual_distance," %02d" , 00);
	
	lv_obj_t * km_h_unity = lv_label_create(screen);
	lv_obj_align(km_h_unity, LV_ALIGN_RIGHT_MID, -15 , -50);
	lv_obj_set_style_text_font(km_h_unity, &lv_font_montserrat_20, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(km_h_unity, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(km_h_unity,"KM/\n  h" );
	
	lv_obj_t * km_unity;
	km_unity = lv_label_create(screen);
	lv_obj_align(km_unity, LV_ALIGN_RIGHT_MID, -15 , 20);
	lv_obj_set_style_text_font(km_unity, &lv_font_montserrat_20, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(km_unity, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(km_unity,"KM");
	
	// Buttons of bottom
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_color_white());
	lv_style_set_border_width(&style, 0);
	lv_style_set_border_color(&style, lv_color_white());
	
	lv_obj_t * btn1 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn1, configscreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn1,55,55);
	lv_obj_align(btn1, LV_ALIGN_BOTTOM_MID, 110 , -5);
	lv_obj_add_style(btn1, &style, 0);

	labelBtn1 = lv_label_create(btn1);
	lv_obj_set_style_text_font(labelBtn1, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn1, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn1,LV_SYMBOL_SETTINGS);
	lv_obj_center(labelBtn1);
	
	lv_obj_t * btn2 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn2, homescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn2,55,55);
	lv_obj_align(btn2,LV_ALIGN_BOTTOM_LEFT, 15 , -5);
	lv_obj_add_style(btn2, &style, 0);

	labelBtn2 = lv_label_create(btn2);
	lv_obj_set_style_text_font(labelBtn2, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn2, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn2,LV_SYMBOL_HOME);
	lv_obj_center(labelBtn2);
	
	lv_obj_t * btn3 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn3, routescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn3,55,55);
	lv_obj_align(btn3,LV_ALIGN_BOTTOM_MID, 0 , -5);
	lv_obj_add_style(btn3, &style, 0);

	labelBtn3 = lv_label_create(btn3);
	lv_obj_set_style_text_font(labelBtn3, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn3, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_CHARGE);
	lv_obj_center(labelBtn3);
}

void lv_screen_2(lv_obj_t * screen) {
	// background
	lv_obj_t * background = lv_img_create(screen);
	lv_img_set_src(background, &white);
	lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
	
	// Top objects
	lv_obj_t * logo_img = lv_img_create(screen);
	lv_img_set_src(logo_img, &logo);
	lv_obj_align(logo_img, LV_ALIGN_TOP_LEFT, 0, 0);
	
	lv_obj_t * label_screen;
	label_screen = lv_label_create(screen);
	lv_obj_align(label_screen, LV_ALIGN_TOP_MID, 0 , 10);
	lv_obj_set_style_text_font(label_screen, &lv_font_montserrat_16, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(label_screen, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(label_screen,LV_SYMBOL_CHARGE " Route");
	
	
	clock_screen2 = lv_label_create(screen);
	lv_obj_align(clock_screen2, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock_screen2, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock_screen2, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock_screen2,"12:05:34");
	
	//Line points
	static lv_point_t line_points[] = { {5, 0}, {315, 0} };
	static lv_point_t vertical_points[] = { {0, 23}, {0, 158} };

	/*Create style of lines*/
	static lv_style_t style_line;
	lv_style_init(&style_line);
	lv_style_set_line_width(&style_line, 3);
	lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_CYAN));
	lv_style_set_line_rounded(&style_line, true);

	// Bottom Line
	lv_obj_t * bottom_line;
	bottom_line = lv_line_create(screen);
	lv_line_set_points(bottom_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(bottom_line, &style_line, 0);
	lv_obj_align(bottom_line, LV_ALIGN_BOTTOM_MID, 0, -60);
	
	// Top Line
	lv_obj_t * top_line;
	top_line = lv_line_create(screen);
	lv_line_set_points(top_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(top_line, &style_line, 0);
	lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 40);
	
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_color_white());
	lv_style_set_border_width(&style, 0);
	lv_style_set_border_color(&style, lv_color_white());
	
	lv_obj_t * vertical_line;
	vertical_line = lv_line_create(screen);
	lv_line_set_points(vertical_line, vertical_points, 2);     /*Set the points*/
	lv_obj_add_style(vertical_line, &style_line, 0);
	lv_obj_align(vertical_line, LV_ALIGN_LEFT_MID, 80, -20);
	
	lv_obj_t * btn4 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn4, playpause_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn4,55,55);
	lv_obj_align(btn4,LV_ALIGN_LEFT_MID, 15 , -45);
	lv_obj_add_style(btn4, &style, 0);
	

	labelBtn4 = lv_label_create(btn4);
	lv_obj_set_style_text_font(labelBtn4, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn4, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn4,LV_SYMBOL_PLAY);
	lv_obj_center(labelBtn4);
	
	lv_obj_t * btn5 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn5, stop_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn5,55,55);
	lv_obj_align(btn5,LV_ALIGN_LEFT_MID, 15 , 20);
	lv_obj_add_style(btn5, &style, 0);

	labelBtn5 = lv_label_create(btn5);
	lv_obj_set_style_text_font(labelBtn5, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn5, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn5,LV_SYMBOL_STOP);
	lv_obj_center(labelBtn5);
	
	lv_obj_t * dist_img = lv_img_create(screen);
	lv_img_set_src(dist_img, &distance);
	lv_obj_align(dist_img, LV_ALIGN_LEFT_MID, 85 , -38);
	
	lv_obj_t * avg_img = lv_img_create(screen);
	lv_img_set_src(avg_img, &average);
	lv_obj_align(avg_img, LV_ALIGN_LEFT_MID, 165 , -35);
	
	
	lv_obj_t * clock_img = lv_img_create(screen);
	lv_img_set_src(clock_img, &clock_sig);
	lv_obj_align(clock_img, LV_ALIGN_LEFT_MID, 248 , -35);
	
	distance_cron = lv_label_create(screen);
	lv_obj_align(distance_cron, LV_ALIGN_LEFT_MID, 95 , 17);
	lv_obj_set_style_text_color(distance_cron, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(distance_cron," %02d" , 0);
	
	lv_obj_t * km_unity;
	km_unity = lv_label_create(screen);
	lv_obj_align(km_unity, LV_ALIGN_LEFT_MID, 105 , 40);
	lv_obj_set_style_text_font(km_unity, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(km_unity, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(km_unity,"KM");
	
	vel_cron = lv_label_create(screen);
	lv_obj_align(vel_cron, LV_ALIGN_LEFT_MID, 184 , 17);
	lv_obj_set_style_text_color(vel_cron, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(vel_cron," %02d" , 0);
	
	lv_obj_t * vel_unity;
	vel_unity = lv_label_create(screen);
	lv_obj_align(vel_unity, LV_ALIGN_LEFT_MID, 182 , 40);
	lv_obj_set_style_text_font(vel_unity, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(vel_unity, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(vel_unity,"KM/h");
	
	time_cron = lv_label_create(screen);
	lv_obj_align(time_cron, LV_ALIGN_LEFT_MID, 260 , 20);
	lv_obj_set_style_text_font(time_cron, &lv_font_montserrat_20, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(time_cron, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(time_cron,"00:00");
	
	lv_obj_t * min_unity;
	min_unity = lv_label_create(screen);
	lv_obj_align(min_unity, LV_ALIGN_LEFT_MID, 270 , 40);
	lv_obj_set_style_text_font(min_unity, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(min_unity, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(min_unity,"MIN");
	
	// Buttons of bottom
	
	lv_obj_t * btn1 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn1, configscreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn1,55,55);
	lv_obj_align(btn1, LV_ALIGN_BOTTOM_MID, 110 , -5);
	lv_obj_add_style(btn1, &style, 0);

	labelBtn1 = lv_label_create(btn1);
	lv_obj_set_style_text_font(labelBtn1, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn1, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn1,LV_SYMBOL_SETTINGS);
	lv_obj_center(labelBtn1);
	
	lv_obj_t * btn2 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn2, homescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn2,55,55);
	lv_obj_align(btn2,LV_ALIGN_BOTTOM_LEFT, 15 , -5);
	lv_obj_add_style(btn2, &style, 0);

	labelBtn2 = lv_label_create(btn2);
	lv_obj_set_style_text_font(labelBtn2, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn2, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn2,LV_SYMBOL_HOME);
	lv_obj_center(labelBtn2);
	
	lv_obj_t * btn3 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn3, routescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn3,55,55);
	lv_obj_align(btn3,LV_ALIGN_BOTTOM_MID, 0 , -5);
	lv_obj_add_style(btn3, &style, 0);

	labelBtn3 = lv_label_create(btn3);
	lv_obj_set_style_text_font(labelBtn3, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn3, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_CHARGE);
	lv_obj_center(labelBtn3);
}

void lv_screen_3(lv_obj_t * screen) {
	// background
	lv_obj_t * background = lv_img_create(screen);
	lv_img_set_src(background, &white);
	lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
	
	// Top objects
	lv_obj_t * logo_img = lv_img_create(screen);
	lv_img_set_src(logo_img, &logo);
	lv_obj_align(logo_img, LV_ALIGN_TOP_LEFT, 0, 0);
	
	lv_obj_t * label_screen;
	label_screen = lv_label_create(screen);
	lv_obj_align(label_screen, LV_ALIGN_TOP_MID, 0 , 10);
	lv_obj_set_style_text_font(label_screen, &lv_font_montserrat_16, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(label_screen, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(label_screen,LV_SYMBOL_SETTINGS " Settings");
	
	clock_screen3 = lv_label_create(screen);
	lv_obj_align(clock_screen3, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock_screen3, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock_screen3, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock_screen3,"12:05:34");
	
	//Line points
	static lv_point_t line_points[] = { {5, 0}, {315, 0} };

	/*Create style of lines*/
	static lv_style_t style_line;
	lv_style_init(&style_line);
	lv_style_set_line_width(&style_line, 3);
	lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_CYAN));
	lv_style_set_line_rounded(&style_line, true);

	// Bottom Line
	lv_obj_t * bottom_line;
	bottom_line = lv_line_create(screen);
	lv_line_set_points(bottom_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(bottom_line, &style_line, 0);
	lv_obj_align(bottom_line, LV_ALIGN_BOTTOM_MID, 0, -60);
	
	// Top Line
	lv_obj_t * top_line;
	top_line = lv_line_create(screen);
	lv_line_set_points(top_line, line_points, 2);     /*Set the points*/
	lv_obj_add_style(top_line, &style_line, 0);
	lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 40);
	
	lv_obj_t * roller1 = lv_roller_create(screen);
	lv_roller_set_options(roller1,
	"16.0''\n"
	"18.0''\n"
	"20.0''\n"
	"24.0''\n"
	"26.0''\n"
	"27.5''\n"
	"700c",
	LV_ROLLER_MODE_INFINITE);

	lv_roller_set_visible_row_count(roller1, 4);
	lv_obj_set_size(roller1,120,120);
	lv_obj_align(roller1, LV_ALIGN_RIGHT_MID, -55, -13);
	lv_obj_add_event_cb(roller1, roller_handler, LV_EVENT_ALL, NULL);

	
	bike_wheel = lv_img_create(screen);
	lv_img_set_src(bike_wheel, &t_1);
	lv_obj_align(bike_wheel,  LV_ALIGN_LEFT_MID, 30, -16);
	
	
	// Buttons of bottom
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_color_white());
	lv_style_set_border_width(&style, 0);
	lv_style_set_border_color(&style, lv_color_white());
	
	lv_obj_t * btn1 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn1, configscreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn1,55,55);
	lv_obj_align(btn1, LV_ALIGN_BOTTOM_MID, 110 , -5);
	lv_obj_add_style(btn1, &style, 0);

	labelBtn1 = lv_label_create(btn1);
	lv_obj_set_style_text_font(labelBtn1, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn1, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn1,LV_SYMBOL_SETTINGS);
	lv_obj_center(labelBtn1);
	
	lv_obj_t * btn2 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn2, homescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn2,55,55);
	lv_obj_align(btn2,LV_ALIGN_BOTTOM_LEFT, 15 , -5);
	lv_obj_add_style(btn2, &style, 0);

	labelBtn2 = lv_label_create(btn2);
	lv_obj_set_style_text_font(labelBtn2, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn2, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn2,LV_SYMBOL_HOME);
	lv_obj_center(labelBtn2);
	
	lv_obj_t * btn3 = lv_btn_create(screen);
	lv_obj_add_event_cb(btn3, routescreen_handler, LV_EVENT_ALL, NULL);
	lv_obj_set_size(btn3,55,55);
	lv_obj_align(btn3,LV_ALIGN_BOTTOM_MID, 0 , -5);
	lv_obj_add_style(btn3, &style, 0);

	labelBtn3 = lv_label_create(btn3);
	lv_obj_set_style_text_font(labelBtn3, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelBtn3, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_CHARGE);
	lv_obj_center(labelBtn3);
}


/************************************************************************/
/* Init Functions                                                       */
/************************************************************************/
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}

void io_init(void) {

	pmc_enable_periph_clk(SENSOR_PIO_ID);
	pmc_enable_periph_clk(VERMELHO_PIO_ID);
	pmc_enable_periph_clk(AZUL_PIO_ID);
	pmc_enable_periph_clk(VERDE_PIO_ID);
	
	pio_configure(VERMELHO_PIO, PIO_OUTPUT_0, VERMELHO_IDX_MASK, PIO_DEFAULT);
	pio_configure(VERDE_PIO, PIO_OUTPUT_0, VERDE_IDX_MASK, PIO_DEFAULT);
	pio_configure(AZUL_PIO, PIO_OUTPUT_0, AZUL_IDX_MASK, PIO_DEFAULT);

	pio_configure(SENSOR_PIO_ID, PIO_INPUT, SENSOR_IDX_MASK, PIO_DEFAULT);
	pio_pull_up(SENSOR_PIO,SENSOR_IDX_MASK,0);
	
	pio_handler_set(SENSOR_PIO, SENSOR_PIO_ID, SENSOR_IDX_MASK, PIO_IT_FALL_EDGE,
	sensor_callback);

	pio_enable_interrupt(SENSOR_PIO, SENSOR_IDX_MASK);

	pio_get_interrupt_status(SENSOR_PIO);


	NVIC_EnableIRQ(SENSOR_PIO_ID);
	NVIC_SetPriority(SENSOR_PIO_ID, 4);
}

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.seccond);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/
float kmh_to_hz(float vel, float raio) {
	float f = vel / (2*PI*raio*3.6);
	return(f);
}

double update_distance_rote(double raio, int pulsos_trajeto){
	double total_distance_cron = 2*PI*raio*pulsos_trajeto  / 1000; //Convertendo de metro para Km
	int total_distance_cron_ = 10*( total_distance_cron - (int) total_distance_cron);
	lv_label_set_text_fmt(distance_cron," %d.%d" , (int) total_distance_cron, total_distance_cron_);
	return total_distance_cron;
}

void update_total_distance(double raio, int pulsos_totais){
	double total_distance = 2*PI*raio*pulsos_totais  / 1000; //Convertendo de metro para Km
	int total_distance1 = 10*( total_distance - (int) total_distance);
	lv_label_set_text_fmt(actual_distance," %d.%d" , (int) total_distance, total_distance1);
}

void update_instantaneous_speed(double raio, int pulsos_totais, int dt){
	static vel_antiga = 0;
	double t = 0.001*dt;
	
	float v = (float) 2*PI*raio*3.6*FATOR_CONVERSAO / t;
	lv_label_set_text_fmt(inst_speed, "%.1f",  v);
	if (v- vel_antiga  > DELTA){
		lv_obj_set_style_text_color(acce_indication, lv_color_make(0x00, 0xff, 0x00), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(acce_indication,LV_SYMBOL_UP);
		pio_clear(VERMELHO_PIO, VERMELHO_IDX_MASK);
		pio_set(VERDE_PIO, VERDE_IDX_MASK);
		pio_clear(AZUL_PIO, AZUL_IDX_MASK);
		
	}
	else if( vel_antiga - v > DELTA){
		lv_obj_set_style_text_color(acce_indication, lv_color_make(0xff, 0x0, 0x00), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(acce_indication, LV_SYMBOL_DOWN);
		pio_set(VERMELHO_PIO, VERMELHO_IDX_MASK);
		pio_clear(VERDE_PIO, VERDE_IDX_MASK);
		pio_clear(AZUL_PIO, AZUL_IDX_MASK);
	}
	else{
		lv_obj_set_style_text_color(acce_indication, lv_color_make(0x00, 0x00, 0xff), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(acce_indication,LV_SYMBOL_MINUS);
		pio_clear(VERMELHO_PIO, VERMELHO_IDX_MASK);
		pio_clear(VERDE_PIO, VERDE_IDX_MASK);
		pio_set(AZUL_PIO, AZUL_IDX_MASK);
	}
	vel_antiga = v;
}

static void task_simulador(void *pvParameters) {

	pmc_enable_periph_clk(ID_PIOC);
	pio_set_output(PIOC, PIO_PC31, 1, 0, 0);

	float vel = 5;
	float f;
	int ramp_up = 1;

	while(1){
		pio_clear(PIOC, PIO_PC31);
		delay_ms(1);
		pio_set(PIOC, PIO_PC31);
		if (ramp_up) {
			//printf("[SIMU] ACELERANDO: %d \n", (int) (10*vel));
			vel += 1;
			} else {
			//printf("[SIMU] DESACELERANDO: %d \n",  (int) (10*vel));
			vel -= 1;
		}
		if (vel >= 10)
		ramp_up = 0;
		else if (vel <= 3)
		ramp_up = 1;

		f = kmh_to_hz(vel, RAIO);
		int t = 965*(1.0/f); //UTILIZADO 965 como multiplicador ao invés de 1000
		//para compensar o atraso gerado pelo Escalonador do freeRTOS
		delay_ms(t);
	}
}

static void task_lcd(void *pvParameters) {
	int px, py;
	scr1  = lv_obj_create(NULL);
	scr2  = lv_obj_create(NULL);
	scr3  = lv_obj_create(NULL);
	lv_screen_1(scr1);
	lv_screen_2(scr2);
	lv_screen_3(scr3);
	lv_scr_load(scr1);

	for (;;)  {
		xSemaphoreTake( xMutexLVGL, portMAX_DELAY);
		lv_tick_inc(50);
		lv_task_handler();
		xSemaphoreGive( xMutexLVGL);
		vTaskDelay(50);
	}
}

static void task_change_screen(void *pvParameters) {
	for (;;)  {
		if (xSemaphoreTake(xSemaphoreScreen1,0)){
			lv_scr_load(scr1);
		}
		if (xSemaphoreTake(xSemaphoreScreen2,0)){
			lv_scr_load(scr2);
		}
		if (xSemaphoreTake(xSemaphoreScreen3,0)){
			lv_scr_load(scr3);
		}
	}
}

static void task_update(void *pvParameters) {
	io_init();
	calendar rtc_initial = {2022, 12, 8, 0, 12, 37 ,0};
	RTT_init(1000,0,NULL);
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_SECEN);
	int rote_status = 0 ,min = 0,sec = 0, pulsos_trajeto = 0, pulsos_totais = 0;
	int dt;
	double total_distance_cron;
	float raio = 0.254;
	float raio_buf;
	
	for(;;) {
		if (xQueueReceive(xQueueRaio,&raio_buf,0)){
			raio = raio_buf;
		}
		if (xQueueReceive(xQueuedt,&dt,0)){
			if (rote_status == PLAY){
				pulsos_trajeto++;
				total_distance_cron = update_distance_rote(raio, pulsos_trajeto);
			}
			pulsos_totais++;
			update_total_distance(raio, pulsos_totais);
			update_instantaneous_speed(raio,pulsos_totais,dt);
		}
		
		if (xSemaphoreTake(xSemaphoreRTC, 0)) {
			uint32_t current_hour, current_min, current_sec;
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			lv_label_set_text_fmt(clock_screen1, "%02d:%02d:%02d", current_hour, current_min, current_sec);
			lv_label_set_text_fmt(clock_screen2, "%02d:%02d:%02d", current_hour, current_min, current_sec);
			lv_label_set_text_fmt(clock_screen3, "%02d:%02d:%02d", current_hour, current_min, current_sec);
	
			if (rote_status == PLAY){
				sec++;
				min = sec  / 60;
				double vel_cron_med =  (double) (total_distance_cron) / (sec / 3600); // velocidade média em km/h
				lv_label_set_text_fmt(time_cron, "%02d:%02d", min, sec % 60);
				lv_label_set_text_fmt(vel_cron, "%02d", (int) vel_cron_med);
			}
			
		}
		if (xQueueReceive(xQueueCronometro,&rote_status,0)){
			if (rote_status == RESET){
				min = 0;
				sec = 0;
				pulsos_trajeto = 0;
				lv_label_set_text_fmt(time_cron, "%02d:%02d", 0, 0);
				lv_label_set_text_fmt(vel_cron, "%02d", 0);
				lv_label_set_text_fmt(distance_cron, "%02d",0);
			}
		}
	}
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	data->point.x = px;
	data->point.y = py;
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	configure_touch();
	configure_lvgl();
	
	xMutexLVGL = xSemaphoreCreateMutex();
	xSemaphoreScreen1 = xSemaphoreCreateBinary();
	xSemaphoreScreen2 = xSemaphoreCreateBinary();
	xSemaphoreScreen3 = xSemaphoreCreateBinary();
	xSemaphoreRTC = xSemaphoreCreateBinary();
	xQueueCronometro = xQueueCreate(2, sizeof(int));
	xQueuedt = xQueueCreate(100, sizeof(int));
	xQueueRaio = xQueueCreate(2, sizeof(float));

	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}
	if (xTaskCreate(task_change_screen, "CHANGE", TASK_CHANGE_STACK_SIZE, NULL, TASK_CHANGE_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create Change Lcd task\r\n");
	}
	
	if (xTaskCreate(task_update, "update", TASK_UPDATE_STACK_SIZE, NULL, TASK_UPDATE_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create rtc task\r\n");
	}
	
	if (xTaskCreate(task_simulador, "SIMUL", TASK_SIMULATOR_STACK_SIZE, NULL, TASK_SIMULATOR_STACK_PRIORITY, NULL) != pdPASS) {
 			printf("Failed to create simul task\r\n");
 	}
 	 	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}

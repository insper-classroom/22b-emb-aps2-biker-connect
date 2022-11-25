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


xSemaphoreHandle xSemaphoreScreen1;
xSemaphoreHandle xSemaphoreScreen2;
xSemaphoreHandle xSemaphoreScreen3;

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

#define TASK_CHANGE_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_CHANGE_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

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
/* lvgl                                                                 */
/************************************************************************/
LV_FONT_DECLARE(lv_font_montserrat_48);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_IMG_DECLARE(logo);
LV_IMG_DECLARE(today);
LV_IMG_DECLARE(acce);
LV_IMG_DECLARE(vmedia);
LV_IMG_DECLARE(button2);

static lv_obj_t * scr1;  // screen 1
static lv_obj_t * scr2;  // screen 2
static lv_obj_t * scr3;  // screen 2

static lv_obj_t * labelBtn1;
static lv_obj_t * labelBtn2;
static lv_obj_t * labelBtn3;

lv_obj_t * inst_speed;
lv_obj_t * actual_distance;

static void event_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}


static void homescreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen1);
// 		c = lv_label_get_text(inst_speed);
// 		temp = atoi(c);
// 		lv_label_set_text_fmt(inst_speed, "%02d", temp -1);
	}
}

static void routescreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen2);
// 		c = lv_label_get_text(inst_speed);
// 		temp = atoi(c);
// 		lv_label_set_text_fmt(inst_speed, "%02d", temp +2);
	}
}

static void configscreen_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		xSemaphoreGive(xSemaphoreScreen3);
// 		c = lv_label_get_text(inst_speed);
// 		temp = atoi(c);
// 		lv_label_set_text_fmt(inst_speed, "%02d", temp +1);
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
	lv_label_set_text_fmt(label_screen,"Instantaneous");
	
	lv_obj_t * clock;
	clock = lv_label_create(screen);
	lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock,"12:05:34");
	
	//Main topics of screen
	lv_obj_t * animimg0 = lv_animimg_create(screen);
	lv_obj_align(animimg0, LV_ALIGN_LEFT_MID, 30, -16);
	lv_animimg_set_src(animimg0, (lv_img_dsc_t **) anim_imgs, 120);
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
	lv_obj_t * acceleration = lv_img_create(screen);
	lv_img_set_src(acceleration, &acce);
	lv_obj_align(acceleration, LV_ALIGN_RIGHT_MID, -120, -50);
	
	// Diary distance
	lv_obj_t * diary_distance = lv_img_create(screen);
	lv_img_set_src(diary_distance, &today);
	lv_obj_align(diary_distance, LV_ALIGN_RIGHT_MID, -110, 20);
	
	
	inst_speed = lv_label_create(screen);
	lv_obj_align(inst_speed, LV_ALIGN_RIGHT_MID, -55 , -50);
	lv_obj_set_style_text_font(inst_speed, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(inst_speed, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(inst_speed," %02d" , 23);
	
	actual_distance = lv_label_create(screen);
	lv_obj_align(actual_distance, LV_ALIGN_RIGHT_MID, -55 , 20);
	lv_obj_set_style_text_font(actual_distance, &lv_font_montserrat_48, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(actual_distance, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(actual_distance," %02d" , 35);
	
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
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_PLAY);
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
	lv_label_set_text_fmt(label_screen,"Route");
	
	lv_obj_t * clock;
	clock = lv_label_create(screen);
	lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock,"12:05:34");
	
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
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_PLAY);
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
	lv_label_set_text_fmt(label_screen,"Settings");
	
	lv_obj_t * clock;
	clock = lv_label_create(screen);
	lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -5 , 12);
	lv_obj_set_style_text_font(clock, &lv_font_montserrat_14, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(clock, lv_color_black(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(clock,"12:05:34");
	
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
	lv_label_set_text_fmt(labelBtn3,LV_SYMBOL_PLAY);
	lv_obj_center(labelBtn3);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

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
		lv_tick_inc(50);
		lv_task_handler();
		vTaskDelay(50);
	}
}

static void task_change_screen(void *pvParameters) {
	for (;;)  {
		if (xSemaphoreTake(xSemaphoreScreen1,1000)){
			lv_scr_load(scr1);
		}
		if (xSemaphoreTake(xSemaphoreScreen2,1000)){
			lv_scr_load(scr2);
		}
		if (xSemaphoreTake(xSemaphoreScreen3,1000)){
			lv_scr_load(scr3);
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
	
	xSemaphoreScreen1 = xSemaphoreCreateBinary();
	xSemaphoreScreen2 = xSemaphoreCreateBinary();
	xSemaphoreScreen3 = xSemaphoreCreateBinary();

	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}
	if (xTaskCreate(task_change_screen, "CHANGE", TASK_CHANGE_STACK_SIZE, NULL, TASK_CHANGE_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create Change Lcd task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}

/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH0 (0)
#define COM_CH1 (1)

/* TASK PRIORITIES */
#define	led	( tskIDLE_PRIORITY + (UBaseType_t)5 )
#define	prijem1	( tskIDLE_PRIORITY + (UBaseType_t)6 )
#define	prijem0	( tskIDLE_PRIORITY + (UBaseType_t)4 )
#define	process	( tskIDLE_PRIORITY + (UBaseType_t)3 )
#define	send ( tskIDLE_PRIORITY + (UBaseType_t)2 )
#define	display	( tskIDLE_PRIORITY + (UBaseType_t)1 )



/* TASKS: FORWARD DECLARATIONS */

static void SerialReceive0_Task(void* pvParameters);
static void SerialReceive1_Task(void* pvParameters);

static void Processing_Task(void* pvParameters);
static void Display_Task(void* pvParameters);
static void led_bar_tsk(void* pvParameters);
static void SerialSend_Task(void* pvParameters);

void main_demo(void);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */

typedef struct AD    
{
	uint8_t ADmin;
	uint8_t ADmax;
	float value;
	float current_value;
	float max_value;
	float min_value;
}AD_izlaz;

static AD_izlaz struktura;
//= { 5, 15, 5, 0, 5 };
static uint8_t stanje_auta = 0;
static uint8_t kanal0 = 0, kanal1 = 0;

/* RECEPTION DATA BUFFER */   
#define R_BUF_SIZE (32)
static uint8_t r_buffer[R_BUF_SIZE];
static uint8_t r_point;
static uint8_t naredba = 0;
static uint8_t MAX = 0;
static uint8_t taster_display_max = 0, taster_display_min = 0;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */     //ne treba
static const uint8_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */
//SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t TXC_BinarySemaphore;    //semafori kojima se odredjeni task obavestava da je nesto poslato sa serijske ili primljeno sa serijske
static SemaphoreHandle_t RXC_BinarySemaphore0;	  // // slanje i primanje karaktera putem serijske se takodje vrsi pomocu interapta
static SemaphoreHandle_t RXC_BinarySemaphore1;
static SemaphoreHandle_t AD_BinarySemaphore;
static SemaphoreHandle_t ispis_BinarySemaphore;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t Send_BinarySemaphore;
static SemaphoreHandle_t Display_BinarySemaphore;

static QueueHandle_t data_queue, dataAD_queue;                  // red kojim ce se slati podaci izmedju taskova
static TimerHandle_t tajmer_displej;

static uint32_t prvProcessRXCInterrupt(void)    //svaki put kad se nesto posalje sa serijske desi se interrupt i predaje se semafor
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0) {// vraca nenultu vrednost ako je dosao interrupt sa kanala nula
		
		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &xHigherPTW) != pdTRUE) {     // kad se desi interrupt predaj semafor
			printf("Greskaisr0 \n");
		}
			kanal0 = 1;
			kanal1 = 0;
	}
	if (get_RXC_status(1) != 0) {// vraca nenultu vrednost ako je dosao interrupt sa kanala jeddan
		
		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &xHigherPTW) != pdTRUE) {
			printf("Greskaisr1 \n");
		}
			
		kanal0 = 0;
		kanal1 = 1;
	}
	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

static uint32_t OnLED_ChangeInterrupt(void) {    //u interaptu samo predamo semafor

	BaseType_t higher_priority_task_woken = pdFALSE;
	printf("Usao u onledchange\n");
	
	if (xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &higher_priority_task_woken) != pdTRUE) {
		printf("Greska\n");
	}

	portYIELD_FROM_ISR((uint32_t)higher_priority_task_woken);
}

static void TimerCallBack(TimerHandle_t timer)
{

	if (send_serial_character((uint8_t)COM_CH0, (uint8_t)'T') != 0) { //SLANJE TRIGGER SIGNALA
		printf("Greska_send \n");
	}

	 uint32_t brojac3 = 0;

	brojac3++;

	if (brojac3 == (uint32_t)20) {
		brojac3 = (uint32_t)0;
		if (xSemaphoreGive(Send_BinarySemaphore, 0) != pdTRUE) {
			printf("GRESKA");
		}
	}


	if (xSemaphoreGive(Display_BinarySemaphore, 0) != pdTRUE) {
		printf("DISPLAY GRESKA SEMAFOR\n");
	}

}


static void Display_Task(void* pvParameters)   //prikaz vrednosti AD konvertora na displej
{

	static uint8_t pom;
	static uint8_t jedinice, desetice, stotine;
	static uint8_t i = 0, j = 0, k = 0, tmp_cifra, tmp_cifra1, z, l;
	static uint16_t tmp_broj = 0, tmp_broj1 = 0;
	static uint16_t tmp_broj2 = 0, tmp_cifra2 = 0;

	for (;;)
	{

		if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) {  
			printf("Greska take\n");
		}




		tmp_broj = (uint8_t) struktura.current_value; 
		i = 0;
		if (tmp_broj < struktura.ADmin) {
			tmp_broj = (uint8_t) struktura.ADmin;
		}
		printf("CURRENT VALUE %d\n", tmp_broj);

		for (z = (uint8_t)6; z <= (uint8_t)8; z++) {
			if (select_7seg_digit((uint8_t)z) != 0) {   
				printf("Greska_select \n");
			}
			if (set_7seg_digit(0x00) != 0) {      
				printf("Greska_set \n");
			}
		}
		if (tmp_broj == (uint8_t) 0) {
			if (select_7seg_digit((uint8_t)8) != 0) {     
				printf("Greska_select \n");
			}
			if (set_7seg_digit(hexnum[0]) != 0) {       
				printf("Greska_set \n");
			}
		}
		else {
			while (tmp_broj != (uint8_t)0) {

				tmp_cifra = (uint8_t)tmp_broj % (uint8_t)10;

				if (select_7seg_digit((uint8_t)8 - i) != 0) {   
					printf("Greska_select \n");
				}
				if (set_7seg_digit(hexnum[tmp_cifra]) != 0) {      
					printf("Greska_set \n");
				}
				tmp_broj = tmp_broj / (uint8_t)10;  
				i++;
			}
		}

		if (taster_display_max == (uint8_t) 1) {        //kada je pritisnut taster za displej na led baru

			taster_display_max = 0;
			tmp_broj1 = (uint8_t) struktura.max_value;
			j = 0;
			if (tmp_broj1 == (uint8_t) 0) {
				if (select_7seg_digit((uint8_t) 5) != 0) {     
					printf("Greska_select \n");
				}
				if (set_7seg_digit(hexnum[0]) != 0) {       
					printf("Greska_set \n");
				}
			}
			else {
				while (tmp_broj1 != (uint8_t)0) {
					tmp_cifra1 = (uint8_t)tmp_broj1 % (uint8_t)10;
					if (select_7seg_digit((uint8_t)5 - j) != 0) {     
						printf("Greska_select \n");
					}
					if (set_7seg_digit(hexnum[tmp_cifra1]) != 0) {       
						printf("Greska_set \n");
					}
					tmp_broj1 = tmp_broj1 / (uint16_t)10;      
					j++;

				}
			}
		}


		if (taster_display_min == (uint8_t)1) {        //kada je pritisnut taster za displej na led baru

			taster_display_min = 0;
			tmp_broj2 = (uint8_t) struktura.min_value;
			for (l = (uint8_t) 0; l <= (uint8_t) 2; l++) {
				if (select_7seg_digit((uint8_t)l) != 0) {   
					printf("Greska_select \n");
				}
				if (set_7seg_digit(0x00) != 0) {      
					printf("Greska_set \n");
				}
			}
			k = 0;
			if (tmp_broj2 == (uint8_t) 0) {
				if (select_7seg_digit((uint8_t)2) != 0) {     
					printf("Greska_select \n");
				}
				if (set_7seg_digit(hexnum[0]) != 0) {       
					printf("Greska_set \n");
				}
			}
			else {
				while (tmp_broj2 != (uint8_t)0) {
					tmp_cifra2 = tmp_broj2 % (uint16_t)10;
					if (select_7seg_digit((uint8_t)2 - k) != 0) {    
						printf("Greska_select \n");
					}
					if (set_7seg_digit(hexnum[tmp_cifra2]) != 0) {       
						printf("Greska_set \n");
					}
					tmp_broj2 = tmp_broj2 / (uint16_t)10;      
					k++;
				}
			}
		}



	}
}


static void SerialSend_Task(void* pvParameters)
{

	static char vrednost[5];
	static int broj;
	static uint16_t tmp_broj, tmp_broj_decimale;
	static uint8_t i = 0, k = 0, j = 0, cifra1, cifra2;
	static uint8_t tmp_cifra = 0;
	static char tmp_str[50], tmp_str1[10];
	static char string_kontinualno[6], string_kontrolisano[6];

	string_kontinualno[0] = 'k';
	string_kontinualno[1] = 'o';
	string_kontinualno[2] = 'n';
	string_kontinualno[3] = 't';
	string_kontinualno[4] = 'i';
	string_kontinualno[5] = '\0';


	string_kontrolisano[0] = 'k';
	string_kontrolisano[1] = 'o';
	string_kontrolisano[2] = 'n';
	string_kontrolisano[3] = 't';
	string_kontrolisano[4] = 'r';
	string_kontrolisano[5] = '\0';

	for (;;)
	{
		if (xSemaphoreTake(Send_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Greska take\n");
		}
		if (naredba == (uint8_t) 0) {
			for (i = 0; i <= (uint8_t)5; i++) {
				{ //KONTINUALNO
					if (send_serial_character((uint8_t)COM_CH1, (uint8_t)string_kontinualno[i]) != 0) { 
						printf("Greska_send \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));

				}


			}

		}
		else if (naredba == (uint8_t) 1) {
			for (i = 0; i <= (uint8_t)5; i++) {
				{ //KONTROLISANO
					if (send_serial_character((uint8_t)COM_CH1, (uint8_t)string_kontrolisano[i]) != 0) { 
						printf("Greska_send \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));

				}


			}

		}
		else {
			;//no action
		}
		k = (uint8_t) 0;
		tmp_broj = (uint16_t)struktura.value; 
		tmp_broj_decimale = (uint16_t) struktura.value * (uint16_t) 100 - (uint16_t) 100 * (uint16_t) tmp_broj; 
		
		if (tmp_broj == (uint8_t) 0) {
			if (send_serial_character((uint8_t)COM_CH1, 48) != 0) { 
				printf("Greska_send \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));



			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)13) != 0) { 
				printf("Greska_send \n");
			}
		}
		else {
			while (tmp_broj != (uint16_t)0) {
				tmp_cifra = (uint8_t)tmp_broj % (uint8_t)10; 
				tmp_broj = tmp_broj / (uint16_t)10; 
				tmp_str1[k] = tmp_cifra + (char)48; 
				k++;
			}
			j = 1;

			if (k != (uint8_t)0) {    
				while (k != (uint8_t)0) {

					if (send_serial_character((uint8_t)COM_CH1, (uint8_t)tmp_str1[k - j]) != 0) { 
						printf("Greska_send \n");
					}
					k--;

					vTaskDelay(pdMS_TO_TICKS(100));
				}


			}


			printf("tmp_broj_decimale %d \n", tmp_broj_decimale);
			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)46) != 0) { 
				printf("Greska_send \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));

			cifra1 = (uint8_t) tmp_broj_decimale / (uint8_t) 10;  
			cifra2 = (uint8_t)tmp_broj_decimale % (uint8_t)10;
			
			if (send_serial_character((uint8_t)COM_CH1, (uint8_t) 48 + cifra1) != 0) { 
				printf("Greska_send \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));

			if (send_serial_character((uint8_t)COM_CH1, (uint8_t) 48 + cifra2) != 0) { 
				printf("Greska_send \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));



			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)13) != 0) { 
				printf("Greska_send \n");
			}
			i = 0;
		}
		

	}
}



static void SerialReceive0_Task(void* pvParameters)     // ovaj task samo primi karakter sa serijske a zatim ga preko reda prosledi ka tasku za obradu
{
	static uint8_t cc;
	static char tmp_str[200], string_queue[200];
	static uint8_t k;
	static uint8_t z = 0;
	uint8_t len;

	for (;;)
	{
		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {
			printf("Greska take\n");
		}
		if (get_serial_character(COM_CH0, &cc) != 0) {
			printf("Greskaget1 \n");
		}
		
		if (cc != (uint8_t)42) { 
			tmp_str[z] = (char)cc;
			z++;

		}
		else {
			tmp_str[z] = '\0';
			z = 0;		

			if (xQueueSend(data_queue, &tmp_str, 0) != pdTRUE) {
				printf("Greskared, slanje\n");
			}
			
		}
	}

}

static void SerialReceive1_Task(void* pvParameters)     // ovaj task samo primi karakter sa serijske a zatim ga preko reda prosledi ka tasku za obradu
{
	static uint8_t cc1 = 0;
	static char tmp_str[100], string_queue[100];
	static uint8_t i = 0, tmp;

	for (;;)
	{
		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
			printf("Greska em take1 \n");
		}
	
		if (get_serial_character(COM_CH1, &cc1) != 0) {
			printf("Greska_get\n");
		}
		printf("karakter koji pristize %c\n", cc1);
		if (cc1 != (uint8_t)42) {
			if (cc1 >= (uint8_t)65 && cc1 <= (uint8_t)90) { //velika slova prebacujemo u mala
				tmp = cc1 + (uint8_t)32;
				tmp_str[i] = (char)tmp;
				i++;
			}
			else {
				tmp_str[i] = (char)cc1;
				i++;
			}
		}
		else {
			tmp_str[i] = '\0';
			i = 0;
			printf("String sa serijske %s \n", tmp_str);
			strcpy(string_queue, tmp_str);
			if (xQueueSend(data_queue, &string_queue, 0) != pdTRUE) {
				printf("Greska_get\n");
			}
			printf("Red za task 3 \n");
		}

	}

}




static void Processing_Task(void* pvParameters)   //obrada podataka
{

	static char niz[7];
	static uint8_t pom;
	static int vrednost = 0, brojac = 0;
	static double suma_uV = 0.0;
	static float suma1;

	for (;;) {

		//prvo primate red
		if (xQueueReceive(data_queue, &niz, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}


														
		
		float pomocna_suma = (float) 0;
		if (niz[0] == 'a' && niz[1] == 'd' && niz[2] == 'm' && niz[3] == 'i' && niz[4] == 'n')
		{
			struktura.ADmin = ((uint8_t)niz[5] - (uint8_t)48) * (uint8_t)10 + ((uint8_t)niz[6] - (uint8_t)48);
			printf("ADmin = %d\n", struktura.ADmin);     // u polju strukture ADmin se sada nalazi data vrednost
		}

		else if (niz[0] == 'a' && niz[1] == 'd' && niz[2] == 'm' && niz[3] == 'a' && niz[4] == 'x')
		{
			struktura.ADmax = ((uint8_t) niz[5] - (uint8_t) 48) * (uint8_t) 10 + ((uint8_t) niz[6] - (uint8_t) 48);

			printf("ADmax = %d\n", struktura.ADmax);    // u polju strukture ADmax se sada nalazi data vrednost
		}

		else if (niz[0] == 'k' && niz[1] == 'o' && niz[2] == 'n' && niz[3] == 't' && niz[4] == 'i' && niz[5] == 'n' && niz[6] == 'u')
		{

			printf("Poslali ste kontinualno\n");    // u polju strukture ADmax se sada nalazi data vrednost
			
			if (stanje_auta == (uint8_t) 1) {
				if (set_LED_BAR((uint8_t)1, 0x07) != 0) {
					printf("Greska_set\n");
				}

			}
			naredba = 0;
		}

		else if (niz[0] == 'k' && niz[1] == 'o' && niz[2] == 'n' && niz[3] == 't' && niz[4] == 'r' && niz[5] == 'o' && niz[6] == 'l')
		{
			printf("Poslali ste kontrolisano\n");     // u polju strukture ADmax se sada nalazi data vrednost
			naredba = 1;


		}
		else if (niz[0] == (char) 48 || niz[0] == (char) 49) {



			int brr = 0, cifra, suma = 0;
			while (brr < 4) {
				if (niz[brr] >= (char) 48 && niz[brr] <= (char) 57) {
					cifra = niz[brr] - 48;
					suma = suma * 10 + cifra;
					suma1 = ((float) struktura.ADmax - (float) struktura.ADmin) * (float) suma;
				}
				brr++;
			}

			suma_uV = (double) suma1 / (double)1023 + (double)struktura.ADmin;
			if (suma_uV < (double) struktura.ADmin){
				suma_uV = (double) struktura.ADmin;
			}
			struktura.current_value = (float) suma_uV;

			if (struktura.current_value > struktura.max_value) {
				struktura.max_value = struktura.current_value;
			}
			if (struktura.current_value < struktura.min_value) {
				struktura.min_value = struktura.current_value;
			}
			if (naredba == (uint8_t) 1) {

				printf("Vrednost: %lf\n", suma_uV);

				if (suma_uV < 12.5) {
					//pali strujno
					if (stanje_auta == (uint8_t) 1){
						if (set_LED_BAR((uint8_t)1, 0X0B) != 0) {
							printf("Greska_set\n");
						}
					}
					else{
						if (set_LED_BAR((uint8_t)1, 0X0A) != 0) {
							printf("Greska_set\n");
						}
					}
				}
				else if (suma_uV > (double) 13.5 && suma_uV < (double) 14) {
					if (stanje_auta == (uint8_t) 1){
						if (set_LED_BAR((uint8_t)1, 0X07) != 0) {
							printf("Greska_set\n");
						}
					}
					else{
						if (set_LED_BAR((uint8_t)1, 0X06) != 0) {
							printf("Greska_set\n");
						}
					}
				}
				else if (suma_uV >= (double) 14) {
					if (stanje_auta == (uint8_t) 1){
						if (set_LED_BAR((uint8_t)1, 0X01) != 0) {
							printf("Greska_set\n");
						}
					}
					else{
						if (set_LED_BAR((uint8_t)1, 0X00) != 0) {
							printf("Greska_set\n");
						}
					}
				}
				else {
					printf(" ");
				}
			}
			else {
				printf(" ");
			}
			if (brojac < 20) {
				vrednost = vrednost + suma;
				brojac++;
			}
			else {
				pomocna_suma = (float) vrednost / (float) 20;
				struktura.value = ((float) struktura.ADmax - (float) struktura.ADmin) * pomocna_suma / (float) 1023 + (float) struktura.ADmin;
				brojac = 0;
				vrednost = 0;
				printf("Usrednjena vrednsot: %.2f\n", struktura.value);

			}
		}
		else if (niz[0] == 'L') {
			printf("LEDOVKE\n");

			stanje_auta = (uint8_t)niz[1] - (uint8_t)48;  //nezavisno od toga da li rezim 0 ili 1
			if (set_LED_BAR((uint8_t)1, stanje_auta) != 0) {
				printf("Greska_set\n");
			}

			taster_display_min = (uint8_t)niz[2] - (uint8_t)48;
			taster_display_max = (uint8_t)niz[3] - (uint8_t)48;
		}
		else {
			printf(" ");
         }

	}

}


static void led_bar_tsk(void* pvParameters) {

	uint8_t led_tmp, tmp_cifra, led_tmp1, i;

	static char tmp_string[20];

	for (;;)
	{
		if (xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Greska sem take\n");
		}
		  
		if (get_LED_BAR((uint8_t)0, &led_tmp) != 0) { 
			printf("Greska_get\n");
		}


		printf("LED_TMP %d \n", led_tmp);
		led_tmp1 = led_tmp; 
		tmp_string[0] = 'L';
		for (i = (uint8_t) 1; i <= (uint8_t) 3; i++) {
			tmp_cifra = led_tmp1 % (uint8_t) 2;
			led_tmp1 = led_tmp1 / (uint8_t) 2;
			tmp_string[i] = tmp_cifra + (char) 48;
		}
		tmp_string[4] = '\0';
		printf("STRING LEDOVKE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s \n", tmp_string);
		if (xQueueSend(data_queue, &tmp_string, 0) != pdTRUE) {
			printf("Greska_get\n");
		}
	}

}


void main_demo(void)
{
	// Init peripherals
	uint8_t i;

	if (init_LED_comm() != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_7seg_comm() != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_uplink(COM_CH0) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_downlink(COM_CH0) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}

	if (init_serial_uplink(COM_CH1) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_downlink(COM_CH1) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}


								 /* LED BAR INTERRUPT HANDLER */
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);  //omogucava inerapt ii definise fju za obradu prekida 

	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);


	// Semaphores
	RXC_BinarySemaphore0 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore0 == NULL) {
		printf("Greskasem\n");
	}

	RXC_BinarySemaphore1 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore1 == NULL) {
		printf("Greskasem\n");
	}

	TXC_BinarySemaphore = xSemaphoreCreateBinary();
	if (TXC_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	AD_BinarySemaphore = xSemaphoreCreateBinary();
	if (AD_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	ispis_BinarySemaphore = xSemaphoreCreateBinary();
	if (ispis_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();
	if (LED_INT_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	Display_BinarySemaphore = xSemaphoreCreateBinary();
	if (Display_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	Send_BinarySemaphore = xSemaphoreCreateBinary();
	if (Send_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	// Queues
	data_queue = xQueueCreate(1, 7u * sizeof(char));
	dataAD_queue = xQueueCreate(2, sizeof(AD_izlaz));

	

	BaseType_t status;
	tajmer_displej = xTimerCreate(     
		"timer",
		pdMS_TO_TICKS(100),
		pdTRUE,
		NULL,
		TimerCallBack
	);


	

	status = xTaskCreate(SerialReceive0_Task, "receive_task", configMINIMAL_STACK_SIZE, NULL, prijem0, NULL);  //task koji prima podatke sa serijske
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(SerialReceive1_Task, "receive_task", configMINIMAL_STACK_SIZE, NULL, prijem1, NULL);  //task koji prima podatke sa serijske
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(SerialSend_Task, "send_task", configMINIMAL_STACK_SIZE, NULL, send, NULL);  
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(Processing_Task, "processing_task", configMINIMAL_STACK_SIZE, NULL, process, NULL);  //task za obradu podataka
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(led_bar_tsk, "led_task", configMINIMAL_STACK_SIZE, NULL, led, NULL);  //task za obradu podataka
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(Display_Task, "display", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)display, NULL);
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	if (xTimerStart(tajmer_displej, 0) != pdPASS) {
		printf("Greska prilikom kreiranja\n");
	}

	struktura.min_value = (float) struktura.ADmax;
	for (i = (uint8_t) 0; i <= (uint8_t) 8; i++) {
		if (select_7seg_digit((uint8_t)i) != 0) {     
			printf("Greska_select \n");
		}
		if (set_7seg_digit(0x00) != 0) {       
			printf("Greska_set \n");
		}
	}
	vTaskStartScheduler();
	for (;;){}
}

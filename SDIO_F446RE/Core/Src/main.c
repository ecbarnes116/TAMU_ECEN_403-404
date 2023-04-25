/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdio.h"
#include <string.h>
#include <math.h>
#include <time.h>
//#include "file_handling.h"
//#include "UartRingbuffer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Total number of samples across each channel (4*1000 = buffer size of 4000 < 4096 maximum)
#define ADC_BUFFER_SIZE 200

#define BUFFER_SIZE 128
//#define PATH_SIZE 32

#define SD_BUFFER_SIZE 2000

#define THRESHOLD_AUDIO 	   40
#define THRESHOLD_PRESSURE 	   40
#define THRESHOLD_ACCELERATION 40

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

RTC_HandleTypeDef hrtc;

SD_HandleTypeDef hsd;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// 16 bit (Half Word) ADC DMA data width
// ADC array for data (size of 5, width of 16 bits)
uint16_t adc_data[ADC_BUFFER_SIZE];
uint16_t SD_data[ADC_BUFFER_SIZE];

uint16_t audio_arr[ADC_BUFFER_SIZE/8];
uint16_t pressure_arr[ADC_BUFFER_SIZE/8];
uint16_t acc_arr[ADC_BUFFER_SIZE/8];
//float acc_arr[ADC_BUFFER_SIZE];

// data_type??? time_arr[ADC_BUFFER_SIZE/8];

static volatile uint16_t *fromADC_Ptr;
static volatile uint16_t *toSD_Ptr = &adc_data[0];

char buffer[BUFFER_SIZE];	// Store strings for f_write
//char path[PATH_SIZE];		// buffer to store path

// Buffer that is written to SD card
char SD_buffer[SD_BUFFER_SIZE];

volatile uint8_t dataReady;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// These are already defined in file_handling.c
FATFS fs; // file system
FIL fil; // file (this needs to be changed)
FILINFO fno;		// ???
FRESULT fresult;
UINT br, bw;
FATFS *pfs;
DWORD fre_clust;
uint32_t total, free_space;

uint8_t count = 0;

// What should I initialize these values to?
// I could just not initialize them to a value, but then
// use a HAL delay once the program starts so there is an
// extremely quick (1 ms) grace period where it isn't checking
// for explosions, but still filling the DMA buffer?
uint8_t explosionDetected = 0;

uint16_t current_audio;
uint16_t current_pressure;
uint16_t current_acc;
//float current_acc;

uint16_t current_acc_x;
uint16_t current_acc_y;

uint16_t previous_audio;
uint16_t previous_pressure;
uint16_t previous_acc;
//float previous_acc;

uint16_t previous_acc_x;
uint16_t previous_acc_y;

uint16_t delta_audio;
uint16_t delta_pressure;
uint16_t delta_acc;
//float delta_acc

int _write(int file, char *ptr, int length) {
	int i = 0;

	for(i = 0; i < length; i++) {
		ITM_SendChar((*ptr++));
	}

	return length;
}

int bufsize (char *buf) {
	int i=0;
	while (*buf++ != '\0') i++;
	return i;
}


// Clear UART buffer for debugging
void bufclear(void) {
	for(int i = 0; i < BUFFER_SIZE; i++){
		buffer[i] = '\0';
	}
}

void SDbufclear(void) {
	for(int i = 0; i < SD_BUFFER_SIZE; i++){
		SD_buffer[i] = '\0';
	}
}

// Size of buffer needs to be a multiple of number of ADC channels (minimum of 5)
// Needs to be divisible by the number of bytes in each line
// that I am writing to the SD card				<-- What did I mean by this???

// Called when ADC buffer is half filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
	fromADC_Ptr = &adc_data[0];
	toSD_Ptr 	= &SD_data[0];

	dataReady = 1;
}



// Called when ADC buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	fromADC_Ptr = &adc_data[ADC_BUFFER_SIZE/2];
	toSD_Ptr 	= &SD_data[ADC_BUFFER_SIZE/2];

	dataReady = 1;
}



void writeSD(const void* buffer) {
	// Moves the file read/write pointer to the end of the file
	fresult = f_lseek(&fil, f_size(&fil));

	// Write the buffer (data worth half of DMA buffer) to the file
	fresult = f_write(&fil, buffer, SD_BUFFER_SIZE, &bw);

	// f_sync flushes the cached information of a writing file
	//
	// Performs the same process as f_close function but the file is left opened
	// and can continue read/write/seek operations to the file
	fresult = f_sync(&fil);
}



void processData() {
	for(uint8_t i = 0; i < (ADC_BUFFER_SIZE)/2; i++) {

		// IF I DIVISIBLE BY CHANNEL NUMBER, THEN THAT VALUE CORRELATES WITH THE NTH SENSOR

		// THE CODE HERE IS MESSED UP. I AM NOT READING EVERY 5 INPUTS LIKE I WANTED TO.
		current_audio = fromADC_Ptr[i];

//		current_pressure = fromADC_Ptr[i+1];
//		current_acc = sqrt(pow(fromADC_Ptr[i+2], 2) + pow(fromADC_Ptr[i+3], 2) + pow(fromADC_Ptr[i+4], 2));
//
//		current_acc_x = fromADC_Ptr[i+2];
//		current_acc_y = fromADC_Ptr[i+3];
//		current_acc_z = fromADC_Ptr[i+4];

		delta_audio = current_audio - previous_audio;
//		delta_pressure = current_pressure - previous_pressure;
//		delta_acc = current_acc - previous_acc;

		// Do explosion detection here
		if(delta_audio >= THRESHOLD_AUDIO) {
			explosionDetected = 1;
		}
//		else if(delta_pressure >= THRESHOLD_PRESSURE) {
//			explosionDetected = 1;
//		}
//		else if(delta_acc >= THRESHOLD_ACCELERATION) {
//			explosionDetected = 1;
//		}

//		fresult = f_lseek(&fil, f_size(&fil));
//		fresult = f_printf(&fil, "%d, %d, %d, %d, %d, d_audio = %d, d_pressure = %d, d_acc = %d\r\n", 0, explosionDetected, current_audio, current_pressure, current_acc, delta_audio, delta_pressure, delta_acc);
//		fresult = f_sync(&fil);

		fresult = f_lseek(&fil, f_size(&fil));
		fresult = f_printf(&fil, "%d, %d, %d, %d, d_audio = %d\r\n", count, i, explosionDetected, current_audio, delta_audio);


		// Logic for determining when to set explosionDetected back to 0
		// Use Friedlander waveform to estimate how long the explosion will last for
		explosionDetected = 0;

		// The current samples will be the "previous" samples for the next samples
		previous_audio = current_audio;
//		previous_pressure = current_pressure;
//		previous_acc = current_acc;
//
//		previous_acc_x = current_acc_x;
//		previous_acc_y = current_acc_y;

	}
	// Add up a bunch of buffers to shove into a huge buffer than is finally
	// written to the SD card after the first half of the DMA buffer is filled
	dataReady = 0;
	// Sync the written data to the SD card
	// THIS STILL NEEDS TO HAPPEN A LOT LESS OFTEN
	fresult = f_sync(&fil);
}








void processDataNew() {
	uint8_t channel = 0;

	// Keeps track of the "global sample" (i.e, every 4 ADC readings)
	uint8_t sample_index = 0;

//	snprintf(SD_buffer, SD_BUFFER_SIZE, "%s", "\r\n");
	snprintf(SD_buffer, SD_BUFFER_SIZE, "%s", "\0"); // Empty char (null char)

	for(uint8_t i = 0; i < (ADC_BUFFER_SIZE)/2; i++) {

		// These if statements will allow me to save the readings of the ADC to the
		// correct sensor variable because each reading is for a different sensor
		//
		// Store the new value read into its respective array
		// audio_arr 	= [0, 30, 56, 70, 56, 30, 0]
		// pressure_arr = [0, 30, 56, 70, 56, 30, 0]
		//
		// index 0 of audio_arr corresponds with the same reading for pressure_arr
		if	   (i == ((channel * 4) + 0)) {
			current_audio = fromADC_Ptr[i];

			audio_arr[sample_index] = current_audio;

			// Get time here (first reading will count as the time the sample was read)
			// Write this value to the temp string when current_audio is written

			// time_arr[sample_index] = some_function_to_get_time_in_micro_seconds()
		}
		else if(i == ((channel * 4) + 1)) {
			current_pressure = fromADC_Ptr[i];

			pressure_arr[sample_index] = current_pressure;

		}
		else if(i == ((channel * 4) + 2)) {
			current_acc_x = fromADC_Ptr[i];
		}
		else if(i == ((channel * 4) + 3)) {
			current_acc_y = fromADC_Ptr[i];

			// current_acc = abs(current_acc_x) + abs(current_acc_y);
			current_acc = current_acc_x;
			acc_arr[sample_index] = current_acc;
		}

		// USE MAGNITUDE
		// Don't take sqroot of int lol, assign to float
//		current_acc = abs(current_acc_x) + abs(current_acc_y);
		current_acc = current_acc_x;

		// Treat every 4th reading like one reading
		if((i % 4) == 3) {
			printf("%d\n", sample_index);

			// Only want to get deltas every 4 reading  on the 4th reading because
			// all values only update after 4 readings (4 values, one per reading)
			if(current_audio > previous_audio) {
				delta_audio = current_audio - previous_audio;
			}
			else {
				delta_audio = 0;
			}
			if(current_pressure > previous_pressure) {
				delta_pressure = current_pressure - previous_pressure;
			}
			else {
				delta_pressure = 0;
			}
			if(current_acc > previous_acc) {
				delta_acc = current_acc - previous_acc;
			}
			else {
				delta_acc = 0;
			}

			// Do explosion detection here
			if(delta_audio >= THRESHOLD_AUDIO) {
				explosionDetected = 1;
			}
			else if(delta_pressure >= THRESHOLD_PRESSURE) {
				explosionDetected = 1;
			}
			else if(delta_acc >= THRESHOLD_ACCELERATION) {
				explosionDetected = 1;
			}

			// Add up a bunch of buffers to shove into a huge buffer that is finally
			// written to the SD card after each half of the DMA buffer is filled

			// Create sample string here with time
			// DO THE STRING HERE

		    /* append new string using length of previously added string */
			snprintf(SD_buffer + strlen(SD_buffer), SD_BUFFER_SIZE - strlen(SD_buffer), "%d, %d, %d, %d, d_audio = %d, %s\r\n", count, i, explosionDetected, current_audio, delta_audio, "yo");

//			snprintf(dest, LOC_MAXLEN, "%s%s", "abc", "def");
//			snprintf(dest + strlen(dest), LOC_MAXLEN - strlen(dest), "%s", "ghi");

//			snprintf(sample_buffer, sample_buffer_size, "%d, %d, %d, %d, d_audio = %d\r\n", count, i, explosionDetected, current_audio, delta_audio);
//		    snprintf(SD_buffer + strlen(SD_buffer), SD_BUFFER_SIZE - strlen(SD_BUFFER), "%s", sample_buffer);

			// snprintf(SD_buffer, SD_buffer_size, "%s, %s, %s, %s, %s", time_buff, explosion_buff, audio_buff, pressure_buff, acc_buff);
			// snprintf(sample_buffer, sample_buffer_size, "%d, %d, %d, %d, d_audio = %d\r\n", count, i, explosionDetected, current_audio, delta_audio);

			// The current samples will be the "previous" samples for the next samples
			// These are placed in this loop for the same reason that the deltas are placed here
			previous_audio = current_audio;
			previous_pressure = current_pressure;
			previous_acc = current_acc;

			previous_acc_x = current_acc_x;
			previous_acc_y = current_acc_y;

			sample_index += 1;
		}


//		fresult = f_lseek(&fil, f_size(&fil));
//		fresult = f_printf(&fil, "%d, %d, %d, %d, d_audio = %d\r\n", count, i, explosionDetected, current_audio, delta_audio);
//		fresult = f_sync(&fil);

		// Use Friedlander waveform to estimate how long the explosion will last for,
		// then set flag to 0 when time reaches that value
		explosionDetected = 0;

		// Increment channel counter to read from next channel
		if(channel < 3) {
			channel += 1;
		}
		else {
			channel = 0;
		}
	}
	// Finally, write huge buffer to SD card
	writeSD(SD_buffer);
	// Clear SD_buffer so new data can be written (next half of DMA buffer)
	SDbufclear();

	dataReady = 0;
}









/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */

  // Start DMA buffer
  // Might need to stop DMA at some point
  HAL_ADC_Start_DMA(&hadc1, (uint16_t*)adc_data, ADC_BUFFER_SIZE);

  // Mount SD card
  fresult = f_mount(&fs, "", 0);

  if(fresult != FR_OK){
	  printf("ERROR in mounting SD card...\n");
  }
  else {
	  printf("SD card mounted successfully...\n");
  }

//  // Check free space on SD card
//  f_getfree("", &fre_clust, &pfs);
//
//  total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
//  printf("SD card total size: \t%lu\n", total);
//  bufclear();
//  free_space = (uint32_t)(fre_clust * pfs->csize * 0.5);
//  printf("SD card free space: \t%lu\n", free_space);
//  bufclear();

  char *name = "adc_data.csv";

  fresult = f_stat(name, &fno);

  if (fresult == FR_OK) {
	  printf("*%s* already exists!!!\n",name);
	  bufclear();
  }
  else {
	  fresult = f_open(&fil, name, FA_CREATE_ALWAYS|FA_READ|FA_WRITE);
  }
	  if(fresult != FR_OK) {
		  printf ("ERROR: no %d in creating file *%s*\n", fresult, name);
		  bufclear();
	  }
	  else {
		  printf ("*%s* created successfully\n",name);
		  bufclear();
	  }

  fresult = f_printf(&fil, "time, explosion, audio, pressure, acceleration\r\n");

  // Get starting tick value (start timer)
  int start = HAL_GetTick();



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  if(dataReady) {

		  processDataNew();

		  // Increment count
		  count++;

	  	  }

	  // Stop when count is a certain value (leads to unmount SD card)
	  if(count >= 100) {
		  break;
	  }

  }

  int stop = HAL_GetTick();

  printf("Total time to write %d values to SD card (WITH printf): %d ms\n", count, (stop - start));

  // Stop ADC DMA and disable ADC
  HAL_ADC_Stop_DMA(&hadc1);

  // Close buffer file
  f_close(&fil);
  if (fresult != FR_OK) {
	  printf ("ERROR: no %d in closing file *%s*\n", fresult, name);
	  bufclear();
  }

  // After while loop when break
  // Unmount SD card
  fresult = f_mount(NULL, "/", 1);
  if (fresult == FR_OK) {
	  printf("SD card unmounted successfully...\n");
	  bufclear();
  }
  else {
	  printf("ERROR: unmounting SD card\n");
	  bufclear();
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_APRIL;
  sDate.Date = 0x17;
  sDate.Year = 0x23;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 18;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

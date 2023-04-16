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
//#include "file_handling.h"
//#include "UartRingbuffer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Number of channels being read by ADC (set to 5 when ready)
#define NUM_CHANNELS 5
// Number of samples per channel
#define NUM_SAMPLES 800
// Total number of samples across each channel (5*800 = buffer size of 4000)
#define TOTAL_SAMPLES (NUM_CHANNELS*NUM_SAMPLES)

#define BUFFER_SIZE 128
//#define PATH_SIZE 32

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

SD_HandleTypeDef hsd;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// 16 bit (Half Word) ADC DMA data width
// ADC array for data (size of 5, width of 16 bits)
uint16_t adc_data[NUM_CHANNELS];
uint16_t SD_data[NUM_CHANNELS];

static volatile uint16_t *fromADC_Ptr;
static volatile uint16_t *toSD_Ptr = &adc_data[0];

char buffer[BUFFER_SIZE];	// Store strings for f_write
//char path[PATH_SIZE];		// buffer to store path

volatile uint8_t dataReady = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_SDIO_SD_Init(void);
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

// Size of buffer needs to be a multiple of number of ADC channels (minimum of 5)
// Needs to be divisible by the number of bytes in each line
// that I am writing to the SD card				<-- What did I mean by this???

// Called when buffer is half filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
	fromADC_Ptr = &adc_data[0];
	toSD_Ptr = &SD_data[0];

	dataReady = 1;
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	fromADC_Ptr = &adc_data[NUM_CHANNELS/2];
	toSD_Ptr = &SD_data[NUM_CHANNELS/2];

	dataReady = 1;
}

uint8_t count = 0;

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
  /* USER CODE BEGIN 2 */

  // Start DMA buffer
  // Might need to stop DMA at some point
  HAL_ADC_Start_DMA(&hadc1, (uint16_t*)adc_data, NUM_CHANNELS);

  // What should I initialize these values to?
  // I could just not initialize them to a value, but then
  // use a HAL delay once the program starts so there is an
  // extremely quick (1 ms) grace period where it isn't checking
  // for explosions, but still filling the DMA buffer?
  uint16_t previous_audio;
  uint16_t previous_pressure;
  uint16_t previous_acc;

  uint16_t previous_acc_x;
  uint16_t previous_acc_y;
  uint16_t previous_acc_z;

  uint16_t current_audio;
  uint16_t current_pressure;
  uint16_t current_acc;

  uint16_t current_acc_x;
  uint16_t current_acc_y;
  uint16_t current_acc_z;

  // Mount SD card
  fresult = f_mount(&fs, "", 0);

  if(fresult != FR_OK){
	  printf("ERROR in mounting SD card...\n");
  }
  else {
	  printf("SD card mounted successfully...\n");
  }

  // Check free space on SD card
  f_getfree("", &fre_clust, &pfs);

  total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
  printf("SD card total size: \t%lu\n", total);
  bufclear();
  free_space = (uint32_t)(fre_clust * pfs->csize * 0.5);
  printf("SD card free space: \t%lu\n", free_space);
  bufclear();

//  char *name = "file_1.txt";
//
//  fresult = f_stat(name, &fno);
//
//  if (fresult == FR_OK) {
//	  printf("*%s* already exists!!!!\n",name);
//  }
//  else {
//	  fresult = f_open(&fil, name, FA_CREATE_ALWAYS|FA_READ|FA_WRITE);
//	  if(fresult != FR_OK) {
//		  printf ("ERROR: no %d in creating file *%s*\n", fresult, name);
//	  }
//	  else {
//		  printf ("*%s* created successfully\n",name);
//	  }
//
//	  strcpy(buffer, "This is file 1 and it says 'Hello from Ethan!'\n");
//
//	  // I THINK THIS IS THE BUFFER AARON WAS TALKING ABOUT
//	  fresult = f_write(&fil, buffer, bufsize(buffer), &bw);
//
//	  if (fresult != FR_OK) {
//	  		  printf ("ERROR: unable to write to *%s*\n", name);
//	  	  }
//
//	  fresult = f_close(&fil);
//
//	  if (fresult != FR_OK) {
//		  printf ("ERROR: no %d in closing file *%s*\n", fresult, name);
//	  }
//  }
//
//  //unmount_sd();
//  fresult = f_mount(NULL, "/", 1);
//  if (fresult == FR_OK) {
//	  printf("SD CARD UNMOUNTED successfully...\n");
//  }
//  else {
//	  printf("error!!! in UNMOUNTING SD CARD\n");
//  }


  char *name = "adc_data.csv";

  fresult = f_stat(name, &fno);

  if (fresult == FR_OK) {
	  printf("*%s* already exists!!!!\n",name);
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



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

//	  // ***************** Testing debugging *****************
//	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
//	  count++;
//	  printf("count = %d \n", count);
//	  HAL_Delay(250);

	  // Initialize respective sensor data variables
	  current_audio = adc_data[0];
	  current_pressure = adc_data[1];
	  current_acc = sqrt(pow(adc_data[2], 2) + pow(adc_data[3], 2) + pow(adc_data[4], 2));

	  current_acc_x = adc_data[2];
	  current_acc_y = adc_data[3];
	  current_acc_z = adc_data[4];

	  while(dataReady == 0) {
		  // Do stuff here with one half of ADC while
		  // other half is being filled?
		  fresult = f_lseek(&fil, f_size(&fil));
		  fresult = f_printf(&fil, "ADC channel 0 (audio) = %d\n", current_audio);
		  fresult = f_sync(&fil);

		  // Increment count
		  count++;
	  	  }

	  dataReady = 0;

	  // The current samples will be the "previous" samples for the next samples
	  previous_audio = current_audio;
	  previous_pressure = current_pressure;
	  previous_acc = current_acc;

	  previous_acc_x = current_acc_x;
	  previous_acc_y = current_acc_y;
	  previous_acc_z = current_acc_z;

	  // Stop when count is a certain value (leads to unmount SD card)
	  if(count == 100) {
		  break;
	  }

  }

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  hadc1.Init.NbrOfConversion = 5;
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

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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

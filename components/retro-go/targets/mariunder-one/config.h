// Target definition
#define RG_TARGET_NAME              "MARIUNDER-ONE"

// Storage
#define RG_STORAGE_DRIVER           1                   // 0 = Host, 1 = SDSPI, 2 = SDMMC, 3 = USB, 4 = Flash
#define RG_STORAGE_HOST             SPI2_HOST           // Used by driver 1 and 2
#define RG_STORAGE_SPEED            SDMMC_FREQ_DEFAULT  // Used by driver 1 and 2
#define RG_STORAGE_ROOT             "/sd"               // Storage mount point

// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_80M
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_MARGIN_TOP        0
#define RG_SCREEN_MARGIN_BOTTOM     0
#define RG_SCREEN_MARGIN_LEFT       0
#define RG_SCREEN_MARGIN_RIGHT      0
#define RG_SCREEN_INIT() \
	ILI9341_CMD(0xC5, 0x1A); /* VCOM */ \
	ILI9341_CMD(0x36, 0xA0); /* Display Rotation  */ \
	ILI9341_CMD(0xB2, 0x05, 0x05, 0x00, 0x33, 0x33);  /* Porch Setting */ \
	ILI9341_CMD(0xB7, 0x05);  /* Gate Control //12.2v   -10.43v */ \
	ILI9341_CMD(0xBB, 0x3F);  /* VCOM */ \
	ILI9341_CMD(0xC0, 0x2c);  /* Power control */ \
	ILI9341_CMD(0xC2, 0x01);  /* VDV and VRH Command Enable */ \
	ILI9341_CMD(0xC3, 0x0F);  /* VRH Set 4.3+( vcom+vcom offset+vdv) */ \
	ILI9341_CMD(0xC4, 0xBE);  /* VDV Set 0v */ \
	ILI9341_CMD(0xC6, 0X01);  /* Frame Rate Control in Normal Mode 111Hz */ \
	ILI9341_CMD(0xD0, 0xA4,0xA1);  /* Power Control 1 */ \
	ILI9341_CMD(0xE8, 0x03);   /* Power Control 1 */ \
	ILI9341_CMD(0xE9, 0x09,0x09,0x08);  /* Equalize time control */ \
	ILI9341_CMD(0xE0, 0xD0,0x05,0x09,0x09,0x08,0x14,0x28,0x33,0x3F,0x07,0x13,0x14,0x28,0x30);   /* Set Gamma */ \
	ILI9341_CMD(0xE1, 0xD0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F, 0x1F);   /* Set Gamma */

// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types
#define RG_GAMEPAD_ADC1_MAP {\
    {RG_KEY_UP,    ADC1_CHANNEL_4, ADC_ATTEN_DB_11, 2048, 4096},\
    {RG_KEY_RIGHT, ADC1_CHANNEL_5, ADC_ATTEN_DB_11, 1024, 2048},\
    {RG_KEY_DOWN,  ADC1_CHANNEL_4, ADC_ATTEN_DB_11, 1024, 2048},\
    {RG_KEY_LEFT,  ADC1_CHANNEL_5, ADC_ATTEN_DB_11, 2048, 4096},\
}

#define RG_GAMEPAD_ADC2_MAP {\
    {RG_KEY_SELECT, ADC2_CHANNEL_6, ADC_ATTEN_DB_11, 0, 10},\
    {RG_KEY_START, ADC2_CHANNEL_6, ADC_ATTEN_DB_11, 900, 1500},\
    {RG_KEY_B, ADC2_CHANNEL_6, ADC_ATTEN_DB_11, 1500, 2200},\
    {RG_KEY_A, ADC2_CHANNEL_6, ADC_ATTEN_DB_11, 2300, 3600},\
}

#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_MENU,   GPIO_NUM_15, GPIO_FLOATING, 0},\
    {RG_KEY_OPTION, GPIO_NUM_16,  GPIO_FLOATING, 0},\
}

// Battery
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_CHANNEL      ADC1_CHANNEL_3
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3500.f) / (4200.f - 3500.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// Status LED
#define RG_GPIO_LED                 GPIO_NUM_2

// SPI Display
#define RG_GPIO_LCD_MISO            GPIO_NUM_13
#define RG_GPIO_LCD_MOSI            GPIO_NUM_11
#define RG_GPIO_LCD_CLK             GPIO_NUM_12
#define RG_GPIO_LCD_CS              GPIO_NUM_10
#define RG_GPIO_LCD_DC              GPIO_NUM_21
#define RG_GPIO_LCD_BCKL            GPIO_NUM_14
#define RG_GPIO_LCD_RST             GPIO_NUM_NC

// SPI SD Card
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_13
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_11
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_12
#define RG_GPIO_SDSPI_CS            GPIO_NUM_9

// External I2S DAC
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_42
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_41
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_47
#define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_48

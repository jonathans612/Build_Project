#include "esp_log.h"
#include "motor_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#define LEFT_ESC_GPIO 27 
#define RIGHT_ESC_GPIO 14
static int max_duty;

// Initialize pwm stuff
void esc_init() {
    // Timer
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_15_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    // Duty
    max_duty = (1 << 15) - 1;

    // Individual motor channels
    ledc_channel_config_t channels[2] = {
        {.gpio_num=LEFT_ESC_GPIO, .speed_mode=LEDC_LOW_SPEED_MODE, .channel=LEDC_CHANNEL_0, .timer_sel=LEDC_TIMER_0},
        {.gpio_num=RIGHT_ESC_GPIO, .speed_mode=LEDC_LOW_SPEED_MODE, .channel=LEDC_CHANNEL_1, .timer_sel=LEDC_TIMER_0}
    };
    for (int i=0;i<2;i++) ledc_channel_config(&channels[i]);
}

// Set motor speed based on channel
void esc_set_us(int channel, int us) {
    int duty = (us * max_duty) / 20000;  // 20 ms period
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void driver(direction_t input)
{
    switch (input)
    {
    case UP:
        ESP_LOGI("DRIVER", "UP");
        esc_set_us(LEDC_CHANNEL_0, 1700);
        esc_set_us(LEDC_CHANNEL_1, 1700);
        break;
    case DOWN:
        ESP_LOGI("DRIVER", "DOWN");
        esc_set_us(LEDC_CHANNEL_0, 1000);
        esc_set_us(LEDC_CHANNEL_1, 1000);
        break;
    case LEFT:
        ESP_LOGI("DRIVER", "LEFT");
        esc_set_us(LEDC_CHANNEL_0, 1700);
        esc_set_us(LEDC_CHANNEL_1, 1000);
        break;
    case RIGHT:
        ESP_LOGI("DRIVER", "RIGHT");
        esc_set_us(LEDC_CHANNEL_0, 1000);
        esc_set_us(LEDC_CHANNEL_1, 1700);
        break;
    }
}

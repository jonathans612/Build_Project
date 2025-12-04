#include "esp_log.h"
#include "motor_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- GPIOs for ESCs ---
#define LEFT_ESC_GPIO  18
#define RIGHT_ESC_GPIO 19
static int max_duty;

// --- PWM initialization ---
void esc_init() {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_15_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    max_duty = (1 << 15) - 1;

    ledc_channel_config_t channels[2] = {
        {.gpio_num=LEFT_ESC_GPIO, .speed_mode=LEDC_LOW_SPEED_MODE, .channel=LEDC_CHANNEL_0, .timer_sel=LEDC_TIMER_0},
        {.gpio_num=RIGHT_ESC_GPIO, .speed_mode=LEDC_LOW_SPEED_MODE, .channel=LEDC_CHANNEL_1, .timer_sel=LEDC_TIMER_0}
    };
    for (int i=0;i<2;i++) ledc_channel_config(&channels[i]);
}

// --- Send pulse to ESC ---
void esc_set_us(ledc_channel_t ch, int us) {
    int duty = (us * max_duty) / 20000;  // 20 ms period
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

void driver(direction_t input) {
    switch(input) {
        case UP:
            ESP_LOGI("DRIVER", "UP");
            // forward 
            break;
        case DOWN:
            ESP_LOGI("DRIVER", "DOWN");
            // back
            break;
        case LEFT:
            ESP_LOGI("DRIVER", "LEFT");
            // turn left
            break;
        case RIGHT:
            ESP_LOGI("DRIVER", "RIGHT");
            // turn right
            break;
    }
}

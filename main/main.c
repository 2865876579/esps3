#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define PUMP_GPIO   GPIO_NUM_10
#define PUMP_ON_SEC 5

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PUMP_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    printf("气泵启动，开始充气...\n");
    gpio_set_level(PUMP_GPIO, 1);

    vTaskDelay(pdMS_TO_TICKS(PUMP_ON_SEC * 1000));

    gpio_set_level(PUMP_GPIO, 0);
    printf("充气完成，气泵已关闭。\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

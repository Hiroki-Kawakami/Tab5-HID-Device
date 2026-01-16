#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int app_main() {
    while (true) {
        printf("HELLO!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

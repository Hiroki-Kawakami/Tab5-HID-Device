#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "bsp_tab5.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"

int app_main() {
    bsp_tab5_init();
    void *fb = bsp_tab5_display_get_frame_buffer(0);
    memset(fb, 0xff, 1280 * 720 * 2);
    bsp_tab5_display_flush(0);
    bsp_tab5_display_set_brightness(80);

    while (true) {
        printf("HELLO!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

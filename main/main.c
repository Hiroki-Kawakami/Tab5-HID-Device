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
        bsp_point_t points[5];
        int touch_num = bsp_tab5_touch_read(points, 5);
        if (touch_num) {
            char str[64] = {}, *sptr = str;
            for (int i = 0; i < touch_num; i++) {
                int step = sprintf(sptr, "(%d, %d) ", points[i].x, points[i].y);
                sptr += step;
            }
            printf("Touch: %s\n", str);
        }

        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}

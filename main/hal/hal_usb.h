#pragma once
#include <stdbool.h>

typedef enum {
    USB_MODE_OFF = 0,
    USB_MODE_MSC,     // SD как флешка
    USB_MODE_CDC,     // виртуальный COM
    USB_MODE_BOTH,    // MSC + CDC
} usb_mode_t;

bool hal_usb_init(usb_mode_t mode);
void hal_usb_deinit(void);
bool hal_usb_is_mounted(void);
usb_mode_t hal_usb_get_mode(void);
void hal_usb_set_mode(usb_mode_t mode);
void hal_usb_task(void *arg);
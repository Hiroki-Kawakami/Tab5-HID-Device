from typing import Any, Type
from dataclasses import dataclass

def keyboard_layout(renderer: Any):
    # Fn Row
    x_offset = 0
    for i, item in enumerate(['ESC', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12', 'DEL']):
        width = 92 if (i == 0) or (i % 3 == 1) else 91
        renderer.key(item, x=x_offset, y=0, width=width, height=60)
        x_offset += width
    y_offset = 60

    # number row
    x_offset = 0
    for i, item in enumerate(['`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Back']):
        width = 123 if i == 13 else 89
        renderer.key(item, x=x_offset, y=y_offset, width=width, height=80)
        x_offset += width
    y_offset += 80

    # tab row
    x_offset = 0
    for i, item in enumerate(['Tab', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\']):
        width = 123 if i == 0 else 89
        renderer.key(item, x=x_offset, y=y_offset, width=width, height=80)
        x_offset += width
    y_offset += 80

    # caps lock row
    x_offset = 0
    for i, item in enumerate(['Caps', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', 'Enter']):
        width = 150 if i == 0 else 151 if i == 12 else 89
        renderer.key(item, x=x_offset, y=y_offset, width=width, height=80)
        x_offset += width
    y_offset += 80

    # shift row
    x_offset = 0
    for i, item in enumerate(['Shift', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 'Shift']):
        width = 195 if i == 0 or i == 11 else 89
        renderer.key(item, x=x_offset, y=y_offset, width=width, height=80)
        x_offset += width
    y_offset += 80

    # bottom row
    x_offset = 0
    for i, item in enumerate(['Ctrl', 'Win', 'Alt', '', 'Alt', 'Win', 'Ctrl']):
        width = 608 if i == 3 else 112
        renderer.key(item, x=x_offset, y=y_offset, width=width, height=80)
        x_offset += width
    y_offset += 80

    # arrows
    renderer.arrows(x=916, y=539, width=344, height=161)

def trackpad_layout(renderer: Any):
    renderer.trackpad(x=1280 / 10 * 3, y=460, width=512, height=260)
    renderer.trackpad_buttons_lr(x=20, y=600, width=344, height=100)

def build(renderer_class: Type):
    normal = renderer_class()
    normal.fill((0, 0, 0))
    keyboard_layout(normal)
    trackpad_layout(normal)
    normal.write('us.normal')

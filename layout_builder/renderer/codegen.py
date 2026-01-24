from typing import Any

class Input:
    type: str
    x: int
    y: int
    width: int
    height: int
    attr: dict[str, Any]

    def __init__(self, type: str, x: int, y: int, width: int, height: int, **kwargs):
        self.type = type
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.attr = kwargs

    def generate(self) -> str:
        generated = f'.type = LAYOUT_INPUT_TYPE_{self.type}, .region = {{ {self.x}, {self.y}, {self.width}, {self.height} }}'
        if self.type == 'KEY':
            generated += f', .key = HID_DEVICE_KEY_{self.attr['item']}'
        return f'{{ {generated} }},'

class Codegen:
    inputs: list[Input]
    ident: str
    title: str
    images: list[str]

    def __init__(self, ident: str, title: str, images: list[str]):
        self.inputs = []
        self.ident = ident
        self.title = title
        self.images = images

    def fill(self, color: tuple[float, float, float]):
        pass

    def key(self, item: str, x: int, y: int, width: int, height: int, **kwargs):
        self.inputs.append(Input('KEY', item=item, x=x, y=y, width=width, height=height))

    def arrows(self, x: int, y: int, width: int, height: int):
        lr_key_width = width // 3
        ud_key_width = width - (lr_key_width * 2)
        lr_key_height = height // 2
        keys = [
            { 'item': 'LEFT' , 'x': x                       , 'y': y + lr_key_height    , 'width': lr_key_width, 'height': lr_key_height + 1 },
            { 'item': 'RIGHT', 'x': x + width - lr_key_width, 'y': y + lr_key_height    , 'width': lr_key_width, 'height': lr_key_height + 1 },
            { 'item': 'UP'   , 'x': x                       , 'y': y                    , 'width': ud_key_width, 'height': lr_key_height     },
            { 'item': 'UP'   , 'x': x                       , 'y': y + lr_key_height + 1, 'width': ud_key_width, 'height': lr_key_height     },
        ]
        for k in keys: self.inputs.append(Input('KEY', **k))

    def trackpad(self, x: int, y: int, width: int, height: int):
        self.inputs.append(Input(type='TRACKPAD', x=x, y=y, width=width, height=height))

    def trackpad_buttons_lr(self, x: int, y: int, width: int, height: int):
        self.inputs.append(Input('KEY', item='MOUSE_BUTTON_1', x=x             , y=y, width=width // 2, height=height))
        self.inputs.append(Input('KEY', item='MOUSE_BUTTON_2', x=x + width // 2, y=y, width=width // 2, height=height))

    def _write_image_file(self, image_name: str):
        jpg_path = f'out/layout_{self.ident}.{image_name}.jpg'
        output_path = f'../main/layouts/image/layout_{self.ident}_{image_name}.c'

        with open(jpg_path, 'rb') as f:
            data = f.read()

        # バイト配列を16バイトごとに整形
        hex_lines = []
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
            hex_lines.append(f'    {hex_str},')

        var_name = f'layout_{self.ident}_{image_name}'

        with open(output_path, 'w') as f:
            f.write('\n'.join([
                '#include "layouts/layout.h"',
                '',
                'static const uint8_t data[] = {',
                *hex_lines,
                '};',
                '',
                f'const layout_image_t {var_name} = ' + '{',
                '    .data = data,',
                '    .size = sizeof(data),',
                '};',
                '',
            ]))

    def _write_impl(self):
        inputs_array = '\n'.join([
            'static const layout_input_t layout_inputs[] = {',
            *(f'    {input.generate()}' for input in self.inputs),
            '};',
        ])
        layout_def = '\n'.join([
            f'static const layout_config_t layout_config = ' + '{',
            f'    .title = "{self.title}",',
            f'    .base_image = &layout_{self.ident}_normal,',
            f'    .active_image = &layout_{self.ident}_active,',
            f'    .inputs = layout_inputs,',
            f'    .count = {len(self.inputs)},',
            '};',
            f'LAYOUT_REGISTER(layout_config)',
        ])

        with open(f'../main/layouts/layout_{self.ident}.c', 'w') as f:
            f.write('\n'.join([
                '#pragma once',
                '#include "hid_device_key.h"',
                '#include "layout.h"',
                '',
                *(f'extern const layout_image_t layout_{self.ident}_{img};' for img in self.images),
                '',
                inputs_array,
                layout_def,
                '',
            ]))

    def write(self):
        for image_name in self.images:
            self._write_image_file(image_name)
        self._write_impl()

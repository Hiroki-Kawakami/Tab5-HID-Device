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
        generated = f'.type = LAYOUT_INPUT_TYPE_{self.type}, .size = {{ {self.x}, {self.y}, {self.width}, {self.height} }}'
        if self.type == 'KEY':
            generated += f', .key = HID_DEVICE_KEY_{self.attr['item']}'
        return f'{{ {generated} }},'

class Codegen:
    inputs: list[Input]

    def __init__(self):
        self.inputs = []

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

    def write(self, filename: str, layoutname: str):
        inputs_array = '\n'.join([
            'static const layout_input_t layout_inputs[] = {',
            *(f'    {input.generate()}' for input in self.inputs),
            '};',
        ])
        layout_def = '\n'.join([
            f'const layout_config_t layout_config = {{',
            f'    .name = "{layoutname}",',
            f'    .inputs = layout_inputs,',
            f'    .count = {len(self.inputs)},',
            '};',
            'LAYOUT_REGISTER(layout_config)',
        ])
        with open(f'out/{filename}.c', 'w') as f:
            f.write('#include "hid_device_key.h"\n')
            f.write('#include "layouts/layout.h"\n\n')
            f.write(inputs_array + '\n')
            f.write(layout_def + '\n')

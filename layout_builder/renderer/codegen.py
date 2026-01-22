from dataclasses import dataclass

@dataclass
class Key:
    item: str
    x: int
    y: int
    width: int
    height: int

@dataclass
class Input:
    type: str
    x: int
    y: int
    width: int
    height: int

class Codegen:
    keyboard_keys: list[Key]
    inputs: list[Input]

    def __init__(self):
        self.keyboard_keys = []
        self.keyboard_pairs = {}
        self.inputs = []

    def fill(self, color: tuple[float, float, float]):
        pass

    def key(self, item: str, x: int, y: int, width: int, height: int, **kwargs):
        self.keyboard_keys.append(Key(item, x, y, width, height))

    def arrows(self, x: int, y: int, width: int, height: int):
        lr_key_width = width // 3
        ud_key_width = width - (lr_key_width * 2)
        lr_key_height = height // 2
        self.keyboard_keys.append(Key('LEFT' , x                       , y + lr_key_height    , lr_key_width, lr_key_height + 1))
        self.keyboard_keys.append(Key('RIGHT', x + width - lr_key_width, y + lr_key_height    , lr_key_width, lr_key_height + 1))
        self.keyboard_keys.append(Key('UP'   , x                       , y                    , ud_key_width, lr_key_height    ))
        self.keyboard_keys.append(Key('DOWN' , x                       , y + lr_key_height + 1, ud_key_width, lr_key_height    ))

    def trackpad(self, x: int, y: int, width: int, height: int):
        self.inputs.append(Input('TRACKPAD', x, y, width, height))

    def trackpad_buttons_lr(self, x: int, y: int, width: int, height: int):
        self.keyboard_keys.append(Key('MOUSE_BUTTON_1', x             , y, width // 2, height))
        self.keyboard_keys.append(Key('MOUSE_BUTTON_2', x + width // 2, y, width // 2, height))

    def write(self, filename: str):
        keyboard_keys_array = '\n'.join([
            'static const layout_key_t layout_keyboard_keys[] = {',
            *(f'    [{i + 1}] = {{ HID_DEVICE_KEY_{key.item}, {key.x}, {key.y}, {key.width}, {key.height} }}' for i, key in enumerate(self.keyboard_keys)),
            '};',
        ])
        inputs_array = '\n'.join([
            'static const layout_input_t layout_inputs[] = {',
            *(f'    {{ LAYOUT_INPUT_TYPE_{input.type}, {input.x}, {input.y}, {input.width}, {input.height} }}' for input in self.inputs),
            '};',
        ])
        layout_def = '\n'.join([
            f'const layout_def_t {filename}_def = {{',
            f'    .keys = {{ {len(self.keyboard_keys)}, layout_keyboard_keys }},',
            f'    .inputs = {{ {len(self.inputs)}, layout_inputs }},',
            '};',
        ])
        with open(f'out/{filename}_def.c', 'w') as f:
            f.write('#include "hid_device_key.h"\n')
            f.write('#include "layouts/layout.h"\n\n')
            f.write(keyboard_keys_array + '\n')
            f.write(inputs_array + '\n')
            f.write(layout_def + '\n')

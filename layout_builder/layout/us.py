from typing import Any, Type, NamedTuple

class Key(NamedTuple):
    item: str
    label: str | None = None
    width: int = 89

class Row(NamedTuple):
    keys: list[Key]
    height: int = 80

KEYBOARD_LAYOUT = [
    # Fn Row
    Row(height=60, keys=[
        Key('ESCAPE'   , label='ESC', width=92),
        Key('F1'       , label='F1' , width=91),
        Key('F2'       , label='F2' , width=92),
        Key('F3'       , label='F3' , width=91),
        Key('F4'       , label='F4' , width=91),
        Key('F5'       , label='F5' , width=92),
        Key('F6'       , label='F6' , width=91),
        Key('F7'       , label='F7' , width=91),
        Key('F8'       , label='F8' , width=92),
        Key('F9'       , label='F9' , width=91),
        Key('F10'      , label='F10', width=91),
        Key('F11'      , label='F11', width=92),
        Key('F12'      , label='F12', width=91),
        Key('DELETE'   , label='DEL', width=92),
    ]),
    # Number Row
    Row(keys=[
        Key('GRAVE'    , label='`'   ),
        Key('1'        , label='1'   ),
        Key('2'        , label='2'   ),
        Key('3'        , label='3'   ),
        Key('4'        , label='4'   ),
        Key('5'        , label='5'   ),
        Key('6'        , label='6'   ),
        Key('7'        , label='7'   ),
        Key('8'        , label='8'   ),
        Key('9'        , label='9'   ),
        Key('0'        , label='0'   ),
        Key('MINUS'    , label='-'   ),
        Key('EQUAL'    , label='='   ),
        Key('BACKSPACE', label='Back', width=123),
    ]),
    # Tab Row
    Row(keys=[
        Key('TAB'          , label='Tab', width=123),
        Key('Q'            , label='Q'  ),
        Key('W'            , label='W'  ),
        Key('E'            , label='E'  ),
        Key('R'            , label='R'  ),
        Key('T'            , label='T'  ),
        Key('Y'            , label='Y'  ),
        Key('U'            , label='U'  ),
        Key('I'            , label='I'  ),
        Key('O'            , label='O'  ),
        Key('P'            , label='P'  ),
        Key('LEFT_BRACKET' , label='['  ),
        Key('RIGHT_BRACKET', label=']'  ),
        Key('BACKSLASH'    , label='\\' ),
    ]),
    # Caps Lock Row
    Row(keys=[
        Key('CAPS_LOCK', label='Caps' , width=150),
        Key('A'        , label='A'    ),
        Key('S'        , label='S'    ),
        Key('D'        , label='D'    ),
        Key('F'        , label='F'    ),
        Key('G'        , label='G'    ),
        Key('H'        , label='H'    ),
        Key('J'        , label='J'    ),
        Key('K'        , label='K'    ),
        Key('L'        , label='L'    ),
        Key('SEMICOLON', label=';'    ),
        Key('QUOTE'    , label='\''   ),
        Key('ENTER'    , label='Enter', width=151),
    ]),
    # Shift Row
    Row(keys=[
        Key('LEFT_SHIFT' , label='Shift', width=195),
        Key('Z'          , label='Z'    ),
        Key('X'          , label='X'    ),
        Key('C'          , label='C'    ),
        Key('V'          , label='V'    ),
        Key('B'          , label='B'    ),
        Key('N'          , label='N'    ),
        Key('M'          , label='M'    ),
        Key('COMMA'      , label=','    ),
        Key('DOT'        , label='.'    ),
        Key('SLASH'      , label='/'    ),
        Key('RIGHT_SHIFT', label='Shift', width=195),
    ]),
    # Bottom Row
    Row(keys=[
        Key('LEFT_CTRL' , label='Ctrl', width=112),
        Key('LEFT_GUI'  , label='Win' , width=112),
        Key('LEFT_ALT'  , label='Alt' , width=112),
        Key('SPACE'     , label=''    , width=608),
        Key('RIGHT_ALT' , label='Alt' , width=112),
        Key('RIGHT_GUI' , label='Win' , width=112),
        Key('RIGHT_CTRL', label='Ctrl', width=112),
    ]),
]

def keyboard_layout(renderer: Any):
    y_offset = 0
    for row in KEYBOARD_LAYOUT:
        x_offset = 0
        for key in row.keys:
            renderer.key(item=key.item, label=key.label, x=x_offset, y=y_offset, width=key.width, height=row.height)
            x_offset += key.width
        y_offset += row.height

    # arrows
    renderer.arrows(x=916, y=539, width=344, height=161)

def trackpad_layout(renderer: Any):
    renderer.trackpad(x=1280 // 10 * 3, y=460, width=512, height=260)
    renderer.trackpad_buttons_lr(x=20, y=600, width=344, height=100)

def build(renderer_class: Type, codegen_class: Type):
    normal = renderer_class(
        key_bg_color = (0.2, 0.2, 0.2),
        key_text_color = (1.0, 1.0, 1.0),
    )
    normal.fill((0, 0, 0))
    keyboard_layout(normal)
    trackpad_layout(normal)
    normal.write('layout_us.normal')

    active = renderer_class(
        key_bg_color = (0.6, 0.6, 0.6),
        key_text_color = (1.0, 1.0, 1.0),
    )
    active.fill((0, 0, 0))
    keyboard_layout(active)
    trackpad_layout(active)
    active.write('layout_us.active')

    code = codegen_class()
    keyboard_layout(code)
    trackpad_layout(code)
    code.write('layout_us', '1. US')

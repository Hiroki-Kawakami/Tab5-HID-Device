import cairo
import math

class DefaultRenderer:
    def __init__(
        self,
        key_bg_color: tuple[float, float, float],
        key_text_color: tuple[float, float, float],
    ):
        self.surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 1280, 720)
        self.ctx = cairo.Context(self.surface)
        self.key_bg_color = key_bg_color
        self.key_text_color = key_text_color

    def fill(self, color: tuple[float, float, float]):
        self.ctx.set_source_rgb(color[0], color[1], color[2])  # 背景色
        self.ctx.paint()

    def _draw_text(
        self,
        text: str,
        x: int, y: int,
        width: int, height: int,
        font_size: int = 24,
        font: str = 'Hiragino Sans',
    ):
        self.ctx.set_source_rgb(*self.key_text_color)
        self.ctx.select_font_face(font, cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
        self.ctx.set_font_size(font_size)

        extents = self.ctx.text_extents(text)
        text_x = x + (width - extents.width) / 2 - extents.x_bearing
        text_y = y + (height - extents.height) / 2 - extents.y_bearing

        self.ctx.move_to(text_x, text_y)
        self.ctx.show_text(text)

    def _separator_horizontal(
        self,
        x: int, y: int, width: int,
    ):
        self.ctx.set_source_rgb(0.4, 0.4, 0.4)
        self.ctx.set_line_width(1)
        self.ctx.move_to(x, y + 0.5)
        self.ctx.line_to(x + width, y + 0.5)
        self.ctx.stroke()

    def _separator_vertical(
        self,
        x: int, y: int, height: int,
    ):
        self.ctx.set_source_rgb(0.4, 0.4, 0.4)
        self.ctx.set_line_width(1)
        self.ctx.move_to(x + 0.5, y)
        self.ctx.line_to(x + 0.5, y + height)
        self.ctx.stroke()

    def _round_rect(
        self,
        x: int, y: int,
        width: int, height: int,
        radius: int,
        label: str = '', font_size: int = 24,
        border_color: tuple[float, float, float] = None,
        border_width: float = 1,
    ):
        # 角丸四角形を描画
        self.ctx.new_path()
        self.ctx.arc(x + radius, y + radius, radius, math.pi, 1.5 * math.pi)
        self.ctx.arc(x + width - radius, y + radius, radius, 1.5 * math.pi, 2 * math.pi)
        self.ctx.arc(x + width - radius, y + height - radius, radius, 0, 0.5 * math.pi)
        self.ctx.arc(x + radius, y + height - radius, radius, 0.5 * math.pi, math.pi)
        self.ctx.close_path()

        # 背景を塗りつぶし
        self.ctx.set_source_rgb(*self.key_bg_color)
        if border_color:
            self.ctx.fill_preserve()
            # クリッピングで内側のみに制限
            self.ctx.save()
            self.ctx.clip()
            # ボーダーを描画
            self.ctx.new_path()
            self.ctx.arc(x + radius, y + radius, radius, math.pi, 1.5 * math.pi)
            self.ctx.arc(x + width - radius, y + radius, radius, 1.5 * math.pi, 2 * math.pi)
            self.ctx.arc(x + width - radius, y + height - radius, radius, 0, 0.5 * math.pi)
            self.ctx.arc(x + radius, y + height - radius, radius, 0.5 * math.pi, math.pi)
            self.ctx.close_path()
            self.ctx.set_source_rgb(*border_color)
            self.ctx.set_line_width(border_width * 2)
            self.ctx.stroke()
            self.ctx.restore()
        else:
            self.ctx.fill()

        # ラベルを中央に描画
        if label:
            self._draw_text(label, x, y, width, height, font_size)

    def key(self, label: str, x: int, y: int, width: int, height: int, **kwargs):
        self._round_rect(x + 2, y + 2, width - 4, height - 4, 6, label, border_color=(0.4, 0.4, 0.4))

    def arrows(self, x: int, y: int, width: int, height: int):
        lr_key_width = (width - 8) // 3
        ud_key_width = (width - 8) - (lr_key_width * 2)
        lr_key_height = height // 2
        self._round_rect(x, y + lr_key_height, lr_key_width, lr_key_height + 1, 6, border_color=(0.4, 0.4, 0.4), label='◀')
        self._round_rect(x + width - lr_key_width, y + lr_key_height, lr_key_width, lr_key_height + 1, 6, border_color=(0.4, 0.4, 0.4), label='▶')
        self._round_rect(x + lr_key_width + 4, y, ud_key_width, height, 6, border_color=(0.4, 0.4, 0.4))
        self._draw_text('▲', x + lr_key_width + 4, y, ud_key_width, lr_key_height)
        self._draw_text('▼', x + lr_key_width + 4, y + lr_key_height + 1, ud_key_width, lr_key_height)
        self._separator_horizontal(x + lr_key_width + 12, y + lr_key_height, ud_key_width - 20)

    def trackpad(self, x: int, y: int, width: int, height: int):
        self._round_rect(x + 2, y + 2, width - 4, height - 4, 6, border_color=(0.4, 0.4, 0.4))

    def trackpad_buttons_lr(self, x: int, y: int, width: int, height: int):
        self._round_rect(x + 1, y + 1, width - 2, height - 2, 6, border_color=(0.4, 0.4, 0.4))
        self._separator_vertical(x + width / 2, y + 10, height - 20)

    def write(self, filename: str):
        # 反時計回りに90度回転して出力
        w, h = self.surface.get_width(), self.surface.get_height()
        rotated = cairo.ImageSurface(cairo.FORMAT_ARGB32, h, w)
        ctx = cairo.Context(rotated)
        ctx.translate(0, w)
        ctx.rotate(-math.pi / 2)
        ctx.set_source_surface(self.surface, 0, 0)
        ctx.paint()
        rotated.write_to_png(f'out/{filename}.png')

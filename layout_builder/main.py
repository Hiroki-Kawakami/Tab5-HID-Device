import renderer.default as default_renderer
import layout.us as us_layout

def main():
    renderer_class = default_renderer.DefaultRenderer
    us_layout.build(renderer_class)

if __name__ == "__main__":
    main()

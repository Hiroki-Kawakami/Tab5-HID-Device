import renderer.default as default_renderer
import renderer.codegen as codegen
import layout.us as us_layout

def main():
    renderer_class = default_renderer.DefaultRenderer
    codegen_class = codegen.Codegen
    us_layout.build(renderer_class, codegen_class)

if __name__ == "__main__":
    main()

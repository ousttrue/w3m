#include <uv.h>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <iostream>
#include <sstream>

uv_tty_t g_tty_in;
uv_signal_t g_signal_resize;

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
  *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

class Renderer {
  ftxui::Screen _screen;
  ftxui::Screen::Cursor _cursor = {0, 0, ftxui::Screen::Cursor::Shape::Block};

public:
  Renderer() : _screen(ftxui::Screen::Create(ftxui::Terminal::Size())) {
    render();
  }

  void onResize() {
    _screen = ftxui::Screen::Create(ftxui::Terminal::Size());
    render();
  }

  void input(const char *p, size_t n) {
    if (n) {
      switch (p[0]) {
      case 'q':
        uv_read_stop((uv_stream_t *)&g_tty_in);
        uv_signal_stop(&g_signal_resize);
        // uv_timer_stop(&g_timer);
        break;

      case 'h':
        --_cursor.x;
        break;
      case 'j':
        ++_cursor.y;
        break;
      case 'k':
        --_cursor.y;
        break;
      case 'l':
        ++_cursor.x;
        break;
      }
    }
    render();
  }

private:
  // Set cursor position for user using tools to insert CJK characters.
  std::string cursor() {
    std::stringstream ss;
    ss << "\x1b[" << (_cursor.y + 1) << ";" << (_cursor.x + 1) << "H";
    return ss.str();
  }

  ftxui::Element dom() {
    using namespace ftxui;
    auto document = //
        vbox({
            hbox({
                text("north-west"),
                filler(),
                text("north-east"),
            }),
            filler(),
            hbox({
                filler(),
                text("center"),
                filler(),
            }),
            filler(),
            hbox({
                text("south-west"),
                filler(),
                text("south-east"),
            }),
        });
    return document;
  }

  void render() {

    // auto screen = Screen::Create(Dimension::Full());
    ftxui::Render(_screen, dom());

    std::cout
        // origin
        << "\x1b[1;1H"
        // render
        << _screen.ToString()
        << cursor()
        //
        << '\0' << std::flush
        //
        ;
  }
};

int main(int argc, char **argv) {

  Renderer r;

  //
  // stdin tty read
  //
  uv_tty_init(uv_default_loop(), &g_tty_in, 0, 1);
  g_tty_in.data = &r;
  uv_read_start((uv_stream_t *)&g_tty_in, &alloc_buffer,
                [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
                  if (nread < 0) {
                    if (nread == UV_EOF) {
                      // end of file
                      uv_close((uv_handle_t *)&g_tty_in, NULL);
                    }
                  } else if (nread > 0) {
                    // process key input
                    ((Renderer *)stream->data)->input(buf->base, nread);
                  }

                  // OK to free buffer as write_data copies it.
                  if (buf->base) {
                    free(buf->base);
                  }
                });

  //
  // SIGWINCH
  //
  uv_signal_init(uv_default_loop(), &g_signal_resize);
  g_signal_resize.data = &r;
  uv_signal_start(
      &g_signal_resize,
      [](uv_signal_t *handle, int signum) {
        ((Renderer *)handle->data)->onResize();
      },
      SIGWINCH);

  uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_RAW);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());

  return 0;
}

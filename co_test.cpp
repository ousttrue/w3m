#include <iostream>
#include "src/func.h"

int main(int argc, char **argv) {

  std::cout << "start" << std::endl;

  using namespace std::literals::chrono_literals;
  auto task = [d = 500ms]() -> FuncCoroutine<int> {
    std::cout << "[task] wait 500ms" << std::endl;
    // co_await d;
    std::cout << "[task] timer done" << std::endl;
    co_return 4;
  };

  auto outer = [task]() -> FuncCoroutine<int> { co_return co_await task(); };

  auto ret = outer();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "handle.done: " << *ret.handle.promise().value << std::endl;

  return 0;
}

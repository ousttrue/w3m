#pragma once

// runtime

extern int CurrentKey;
extern int prev_key;
extern char *CurrentKeyData;
extern char *CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;
extern void pushEvent(int cmd, void *data);

void resize_hook(int);
const char *searchKeyData();
int searchKeyNum(void);

class App {
  App();

public:
  ~App();
  App(const App &) = delete;
  App &operator=(const App &) = delete;
  static App &instance() {
    static App s_instance;
    return s_instance;
  }

  void initialize();
  int mainLoop();
};

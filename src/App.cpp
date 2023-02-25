#include <asio.hpp>
#include <asio/posix/stream_descriptor.hpp>

#include <plog/Log.h>

#include "App.h"
#include "Args.h"
#include "core.h"

#include "fm.h"
#include "proto.h"
#include <functional>
#include <unordered_set>

const auto PREC_LIMIT = 10000;

struct Event {
  int cmd;
  void *data;
  Event *next;
};
Event *CurrentEvent = nullptr;
Event *LastEvent = nullptr;

class AlarmEvent {
  asio::steady_timer timer;
  int cmd;
  void *data;
  AlarmEvent(asio::io_context &io, std::chrono::milliseconds time)
      : timer(io, time) {}

public:
  static void async_wait(asio::io_context &io, std::chrono::milliseconds time,
                         int cmd, void *data) {
    auto alarm = std::shared_ptr<AlarmEvent>(new AlarmEvent(io, time));
    alarm->cmd = cmd;
    alarm->data = data;
    auto completion_handler = [alarm](asio::error_code) {
      CurrentKey = -1;
      CurrentKeyData = NULL;
      CurrentCmdData = (char *)alarm->data;
      w3mFuncList[alarm->cmd].func();
      CurrentCmdData = NULL;
    };
    alarm->timer.async_wait(completion_handler);
  }
};

class EventDispatcher {
public:
  asio::io_context io_;
  asio::signal_set signals_;
  EventDispatcher() : signals_(io_, SIGWINCH) { async_signal(); }
  void async_signal() {
    signals_.async_wait(std::bind(&EventDispatcher::handler, this,
                                  std::placeholders::_1,
                                  std::placeholders::_2));
  }

  void handler(const asio::error_code &error, int signal_number) {
    // assert(signal_number == SIGWINCH);
    if (error) {
      PLOG_ERROR << error;
    }

    switch (signal_number) {
    case SIGWINCH: {
      setlinescols();
      setupscreen();
      if (CurrentTab) {
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
      }
    } break;

    default:
      throw std::runtime_error("unknown signal");
    }

    // next
    async_signal();
  }

  void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                       clen_t size) {
    auto d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
      save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = TRUE;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
      LastDL->next = d;
    else
      FirstDL = d;
    LastDL = d;
    // add_download_list = TRUE;

    auto completion_handler = [](asio::error_code) { ldDL(); };

    asio::post(asio::bind_executor(
        io_, std::bind(
                 std::forward<decltype(completion_handler)>(completion_handler),
                 asio::error::invalid_argument)));
  }

  void pushEvent(int cmd, void *data) {
    Event *event;

    event = New(Event);
    event->cmd = cmd;
    event->data = data;
    event->next = NULL;
    if (CurrentEvent)
      LastEvent->next = event;
    else
      CurrentEvent = event;
    LastEvent = event;

    auto completion_handler = [](asio::error_code) {
      /* event processing */
      if (CurrentEvent) {
        CurrentKey = -1;
        CurrentKeyData = nullptr;
        CurrentCmdData = (char *)CurrentEvent->data;
        w3mFuncList[CurrentEvent->cmd].func();
        CurrentCmdData = nullptr;
        CurrentEvent = CurrentEvent->next;
        // continue;
      }
    };

    asio::post(asio::bind_executor(
        io_, std::bind(
                 std::forward<decltype(completion_handler)>(completion_handler),
                 asio::error::invalid_argument)));
  }
};

App::App() : dispatcher_(new EventDispatcher) {}

App::~App() {}

App &App::instance() {
  static App s_instance;
  return s_instance;
}

void App::addDownloadList(pid_t pid, char *url, char *save, char *lock,
                          clen_t size) {

  dispatcher_->addDownloadList(pid, url, save, lock, size);
}

void App::pushEvent(int cmd, void *data) { dispatcher_->pushEvent(cmd, data); }

void App::setAlarmEvent(int sec, AlarmEventType status, int cmd, void *data) {
  AlarmEvent::async_wait(dispatcher_->io_,
                         std::chrono::milliseconds(sec * 1000), cmd, data);
}

void App::run(Args &argument) {
  wc_uint8 auto_detect = WcOption.auto_detect;

  Str err_msg = Strnew();
  Buffer *newbuf = NULL;
  InputStream redin;
  if (argument.load_argc == 0) {
    // no URL specified
    char *p;
    if (!isatty(0)) {
      redin = newFileStream(fdopen(dup(0), "rb"), (void (*)(void *))pclose);
      newbuf = openGeneralPagerBuffer(redin);
      dup2(1, 0);
    } else if (argument.load_bookmark) {
      newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
      if (newbuf == NULL)
        Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
    } else if (argument.visual_start) {
      /* FIXME: gettextize? */
      Str s_page;
      s_page =
          Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
      Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
      Strcat_m_charp(
          s_page, "w3m</a>!<p><p>This is w3m version ", w3m_version,
          "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
          NULL);
      newbuf = loadHTMLString(s_page);
      if (newbuf == NULL)
        Strcat_charp(err_msg, "w3m: Can't load string.\n");
      else if (newbuf != NO_BUFFER)
        newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    } else if ((p = getenv("HTTP_HOME")) != NULL ||
               (p = getenv("WWW_HOME")) != NULL) {
      newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
      if (newbuf == NULL)
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
      else if (newbuf != NO_BUFFER)
        pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
    } else {
      if (fmInitialized)
        fmTerm();
      usage();
    }
    if (newbuf == NULL) {
      if (fmInitialized)
        fmTerm();
      if (err_msg->length)
        fprintf(stderr, "%s", err_msg->ptr);
      w3m_exit(2);
    }
    argument.i = -1;
  } else {
    argument.i = 0;
  }

  FormList *request;
  int i = argument.i;
  for (; i < argument.load_argc; i++) {
    if (i >= 0) {
      SearchHeader = argument.search_header;
      DefaultType = argument.default_type;
      char *url;

      url = argument.load_argv[i];
      if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
        url = file_to_url(argument.load_argv[i]);
      else
        url = url_encode(conv_from_system(argument.load_argv[i]), NULL, 0);
      if (w3m_dump == DUMP_HEAD) {
        request = New(FormList);
        request->method = FORM_METHOD_HEAD;
        newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
      } else {
        if (argument.post_file && i == 0) {
          FILE *fp;
          Str body;
          if (!strcmp(argument.post_file, "-"))
            fp = stdin;
          else
            fp = fopen(argument.post_file, "r");
          if (fp == NULL) {
            /* FIXME: gettextize? */
            Strcat(err_msg,
                   Sprintf("w3m: Can't open %s.\n", argument.post_file));
            continue;
          }
          body = Strfgetall(fp);
          if (fp != stdin)
            fclose(fp);
          request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
          request->body = body->ptr;
          request->boundary = NULL;
          request->length = body->length;
        } else {
          request = NULL;
        }
        newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
      }
      if (newbuf == NULL) {
        /* FIXME: gettextize? */
        Strcat(err_msg,
               Sprintf("w3m: Can't load %s.\n", argument.load_argv[i]));
        continue;
      } else if (newbuf == NO_BUFFER)
        continue;
      switch (newbuf->real_scheme) {
      case SCM_MAILTO:
        break;
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
      default:
        pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
        break;
      }
    } else if (newbuf == NO_BUFFER)
      continue;
    if (newbuf->pagerSource ||
        (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source &&
         newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
      newbuf->search_header = argument.search_header;
    if (CurrentTab == NULL) {
      FirstTab = LastTab = CurrentTab = newTab();
      nTab = 1;
      Firstbuf = Currentbuf = newbuf;
    } else if (argument.open_new_tab) {
      _newT();
      Currentbuf->nextBuffer = newbuf;
      delBuffer(Currentbuf);
    } else {
      Currentbuf->nextBuffer = newbuf;
      Currentbuf = newbuf;
    }
    if (!w3m_dump || w3m_dump == DUMP_BUFFER) {
      if (Currentbuf->frameset != NULL && RenderFrame)
        rFrame();
    }
    if (w3m_dump)
      do_dump(Currentbuf);
    else {
      Currentbuf = newbuf;
      saveBufferInfo();
    }
  }
  if (w3m_dump) {
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    save_cookies();
    w3m_exit(0);
  }

  // if (App::instance().add_download_list) {
  //   App::instance().add_download_list = false;
  //   CurrentTab = LastTab;
  //   if (!FirstTab) {
  //     FirstTab = LastTab = CurrentTab = newTab();
  //     nTab = 1;
  //   }
  //   if (!Firstbuf || Firstbuf == NO_BUFFER) {
  //     Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
  //     Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
  //     Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
  //   } else
  //     Currentbuf = Firstbuf;
  //   ldDL();
  // } else
  { CurrentTab = FirstTab; }

  if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
    if (newbuf == NO_BUFFER) {
      if (fmInitialized)
        /* FIXME: gettextize? */
        inputChar("Hit any key to quit w3m:");
    }
    if (fmInitialized)
      fmTerm();
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    if (newbuf == NO_BUFFER) {
      save_cookies();
      if (!err_msg->length)
        w3m_exit(0);
    }
    w3m_exit(2);
  }
  if (err_msg->length)
    disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

  SearchHeader = FALSE;
  DefaultType = NULL;
  UseContentCharset = TRUE;
  WcOption.auto_detect = auto_detect;

  Currentbuf = Firstbuf;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  if (argument.line_str) {
    _goLine(argument.line_str);
  }

  main_loop();
}

class Reader {
  asio::posix::stream_descriptor stream_in_;
  char ch_[1];

public:
  Reader(asio::io_context &io) : stream_in_(io, gettty()) { async_read(); }

  void async_read() {
    asio::async_read(stream_in_, asio::buffer(ch_),
                     std::bind(&Reader::onRead, this, std::placeholders::_1,
                               std::placeholders::_2));
  }

  void onRead(std::error_code error, std::size_t n) {
    assert(n == 1);
    int c = ch_[0];
    if (IS_ASCII(c)) { /* Ascii */
      if (('0' <= c) && (c <= '9') &&
          (prec_num || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
        prec_num = prec_num * 10 + (int)(c - '0');
        if (prec_num > PREC_LIMIT)
          prec_num = PREC_LIMIT;
      } else {
        set_buffer_environ(Currentbuf);
        save_buffer_position(Currentbuf);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = nullptr;

    async_read();
  }
};

void App::main_loop() {

  Reader reader(dispatcher_->io_);

  for (;;) {
    if (Currentbuf->submit) {
      Anchor *a = Currentbuf->submit;
      Currentbuf->submit = nullptr;
      gotoLine(Currentbuf, a->start.line);
      Currentbuf->pos = a->start.pos;
      _followForm(TRUE);
      continue;
    }

    if (activeImage && displayImage && Currentbuf->img &&
        !Currentbuf->image_loaded) {
      loadImage(Currentbuf, IMG_FLAG_NEXT);
    }

    dispatcher_->io_.poll();
  }
}

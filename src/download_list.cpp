#include <memory>
#include <list>
#include "config.h"
#include "download_list.h"
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "frontend/event.h"
#include "frontend/terminal.h"
#include "html/html_context.h"
#include "html/parsetag.h"
#include "w3m.h"
#include "commands.h"
#include "indep.h"
#include "file.h"

//
// Download
//
struct DownloadList
{
    pid_t pid;
    char *url;
    char *save;
    char *lock;
    clen_t size;
    time_t time;
    int running;
    int err;
};
using DownloadListPtr = std::shared_ptr<DownloadList>;
static std::list<DownloadListPtr> FirstDL;

void sig_chld(int signo)
{
    int p_stat;
    pid_t pid;

#ifdef HAVE_WAITPID
    while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0)
#elif HAVE_WAIT3
    while ((pid = wait3(&p_stat, WNOHANG, NULL)) > 0)
#else
    if ((pid = wait(&p_stat)) > 0)
#endif
    {
        if (WIFEXITED(p_stat))
        {
            for (auto &d : FirstDL)
            {
                if (d->pid == pid)
                {
                    d->err = WEXITSTATUS(p_stat);
                    break;
                }
            }
        }
    }
    mySignal(SIGCHLD, sig_chld);
    return;
}

void stopDownload()
{
    for (auto &d : FirstDL)
    {
        if (!d->running)
            continue;
#ifndef __MINGW32_VERSION
        kill(d->pid, SIGKILL);
#endif
        unlink(d->lock);
    }
}

void download_action(tcb::span<parsed_tagarg> _arg)
{
    // DownloadList *d;

    for (auto &arg : _arg)
    {
        pid_t pid;
        if (!strncmp(arg.arg, "stop", 4))
        {
            pid = (pid_t)atoi(&arg.arg[4]);
#ifndef __MINGW32_VERSION
            kill(pid, SIGKILL);
#endif
        }
        else if (!strncmp(arg.arg, "ok", 2))
            pid = (pid_t)atoi(&arg.arg[2]);
        else
            continue;
        for (auto it = FirstDL.begin(); it != FirstDL.end();)
        {
            auto d = *it;
            if (d->pid == pid)
            {
                unlink(d->lock);
                it = FirstDL.erase(it);
                break;
            }
            else
            {
                ++it;
            }
        }
    }
    ldDL(&w3mApp::Instance(), {});
}

int checkDownloadList()
{
    for (auto &d : FirstDL)
    {
        struct stat st;
        if (d->running && !lstat(d->lock, &st))
            return true;
    }
    return false;
}

static bool s_add_download_list = false;
int add_download_list()
{
    return s_add_download_list;
}
void set_add_download_list(int add)
{
    s_add_download_list = add;
}

void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size)
{
    auto d = std::make_shared<DownloadList>();
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(w3mApp::Instance().CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = true;
    d->err = 0;
    FirstDL.push_back(d);
    set_add_download_list(true);
}

static char *convert_size3(clen_t size)
{
    Str tmp = Strnew();
    int n;

    do
    {
        n = size % 1000;
        size /= 1000;
        tmp = Sprintf((char *)(size ? ",%.3d%s" : "%d%s"), n, tmp->ptr);
    } while (size);
    return tmp->ptr;
}

BufferPtr DownloadListBuffer(w3mApp *w3m, const CommandContext &context)
{
    Str src = NULL;
    struct stat st;
    time_t cur_time;
    int duration, rate, eta;
    size_t size;

    cur_time = time(0);
    /* FIXME: gettextize? */
    src = Strnew_m_charp("<html><head><title>", w3m->DOWNLOAD_LIST_TITLE,
                         "</title></head>\n<body><h1 align=center>", w3m->DOWNLOAD_LIST_TITLE, "</h1>\n"
                                                                                               "<form method=internal action=download><hr>\n");
    for (auto it = FirstDL.rbegin(); it!=FirstDL.rend(); ++it)
    {
        auto d = *it;
        if (lstat(d->lock, &st))
            d->running = false;
        src->Push("<pre>\n");
        src->Push(Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
                          html_quote(conv_from_system(d->save))));
        duration = cur_time - d->time;
        if (!stat(d->save, &st))
        {
            size = st.st_size;
            if (!d->running)
            {
                if (!d->err)
                    d->size = size;
                duration = st.st_mtime - d->time;
            }
        }
        else
            size = 0;
        if (d->size)
        {
            int i, l = Terminal::columns() - 6;
            if (size < d->size)
                i = 1.0 * l * size / d->size;
            else
                i = l;
            l -= i;
            while (i-- > 0)
                src->Push('#');
            while (l-- > 0)
                src->Push('_');
            src->Push('\n');
        }
        if ((d->running || d->err) && size < d->size)
            src->Push(Sprintf("  %s / %s bytes (%d%%)",
                              convert_size3(size), convert_size3(d->size),
                              (int)(100.0 * size / d->size)));
        else
            src->Push(Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0)
        {
            rate = size / duration;
            src->Push(Sprintf("  %02d:%02d:%02d  rate %s/sec",
                              duration / (60 * 60), (duration / 60) % 60,
                              duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate)
            {
                eta = (d->size - size) / rate;
                src->Push(Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                                  (eta / 60) % 60, eta % 60));
            }
        }
        src->Push('\n');
        if (!d->running)
        {
            src->Push(Sprintf("<input type=submit name=ok%d value=OK>",
                              d->pid));
            switch (d->err)
            {
            case 0:
                if (size < d->size)
                    src->Push(" Download ended but probably not complete");
                else
                    src->Push(" Download complete");
                break;
            case 1:
                src->Push(" Error: could not open destination file");
                break;
            case 2:
                src->Push(" Error: could not write to file (disk full)");
                break;
            default:
                src->Push(" Error: unknown reason");
            }
        }
        else
            src->Push(Sprintf("<input type=submit name=stop%d value=STOP>",
                              d->pid));
        src->Push("\n</pre><hr>\n");
    }
    src->Push("</form></body></html>");

    return loadHTMLStream(URL::Parse("w3m://download"), StrStream::Create(src->ptr), WC_CES_UTF_8, true);
}

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <pulse/pulseaudio.h>
#include <sys/stat.h>

#define STOP ((void *)(1))

#define SINK_NAME       "@DEFAULT_SINK@"
#define SOURCE_NAME     "@DEFAULT_SOURCE@"
#define MICROPHONE_LOCK "mute_microphone_lock"

static pa_mainloop_api *api = NULL;

static void error(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  fputs("mute: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
}

static char *lock_file(char *path, size_t size) {
  int written = 0;
  char *cache_home = getenv("XDG_CACHE_HOME");

  if (cache_home != NULL) {
    written = snprintf(path, size, "%s/%s", cache_home, MICROPHONE_LOCK);
  } else {
    written = snprintf(path, size, "%s/.cache/%s", getenv("HOME"), MICROPHONE_LOCK);
  }
  return (written < 0 || (unsigned)(written) >= size) ? NULL : path;
}

static void lock_microphone(void) {
  int fd;
  char buffer[PATH_MAX];
  char *path = lock_file(buffer, sizeof(buffer));

  if (path != NULL && (fd = open(path, O_RDWR | O_CREAT, S_IRWXU)) != -1) {
    close(fd);
  }
}

static void unlock_microphone(void) {
  char buffer[PATH_MAX];
  char *path = lock_file(buffer, sizeof(buffer));

  if (path != NULL) {
    remove(path);
  }
}

static bool is_microphone_locked(void) {
  char buffer[PATH_MAX];
  char *path = lock_file(buffer, sizeof(buffer));

  return (path != NULL) && (access(path, F_OK) == 0);
}

static void set_status(int status) {
  api->quit(api, status);
}

static void complete_callback(pa_context *context, int success, void *disconnect) {
  if (!success) {
    error("failure: %s", pa_strerror(pa_context_errno(context)));
    set_status(EXIT_FAILURE);
  }
  if (disconnect) {
    pa_context_disconnect(context);
  }
}

static void microphone_callback(pa_context *context, const pa_source_info *info, int eol, void *userdata) {
  if (eol < 0) {
    error("failed to get source information: %s", pa_strerror(pa_context_errno(context)));
    set_status(EXIT_FAILURE);
    return;
  }
  if (eol == 0) {
    if (info->mute) {
      unlock_microphone();
      pa_operation_unref(pa_context_set_sink_mute_by_name(context, SINK_NAME, 0, complete_callback, NULL));
    } else {
      lock_microphone();
    }
    pa_operation_unref(pa_context_set_source_mute_by_name(context, SOURCE_NAME, !info->mute, complete_callback, STOP));
  }
}

static void audio_callback(pa_context *context, const pa_sink_info *info, int eol, void *userdata) {
  if (eol < 0) {
    error("failed to get sink information: %s", pa_strerror(pa_context_errno(context)));
    set_status(EXIT_FAILURE);
    return;
  }
  if (eol == 0) {
    if (!info->mute) {
      pa_operation_unref(pa_context_set_source_mute_by_name(context, SOURCE_NAME, 1, complete_callback, NULL));
    } else if (!is_microphone_locked()) {
      pa_operation_unref(pa_context_set_source_mute_by_name(context, SOURCE_NAME, 0, complete_callback, NULL));
    }
    pa_operation_unref(pa_context_set_sink_mute_by_name(context, SINK_NAME, !info->mute, complete_callback, STOP));
  }
}

static void context_state_callback(pa_context *context, void *userdata) {
  pa_operation *op = NULL;
  bool microphone = *(bool *)userdata;

  switch (pa_context_get_state(context)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;

    case PA_CONTEXT_READY:
      if (microphone) {
        op = pa_context_get_source_info_by_name(context, SOURCE_NAME, microphone_callback, NULL);
      } else {
        op = pa_context_get_sink_info_by_name(context, SINK_NAME, audio_callback, NULL);
      }
      if (op != NULL) {
        pa_operation_unref(op);
      }
      break;

    case PA_CONTEXT_TERMINATED:
      set_status(EXIT_SUCCESS);
      break;
    case PA_CONTEXT_FAILED:
      error("connection failure: %s", pa_strerror(pa_context_errno(context)));
      set_status(EXIT_FAILURE);
      break;
  }
}

static int toggle(bool microphone) {
  int status = EXIT_FAILURE;

  pa_mainloop *m = NULL;
  pa_context *context = NULL;

  if ((m = pa_mainloop_new()) == NULL) {
    error("pa_mainloop_new failed");
    goto quit;
  }
  if ((api = pa_mainloop_get_api(m)) == NULL) {
    error("pa_mainloop_get_api failed");
    goto quit;
  }
  if ((context = pa_context_new(api, NULL)) == NULL) {
    error("pa_context_new failed");
    goto quit;
  }
  pa_context_set_state_callback(context, context_state_callback, &microphone);
  if (pa_context_connect(context, NULL, 0, NULL) < 0) {
    error("pa_context_connect failed: %s", pa_strerror(pa_context_errno(context)));
    goto quit;
  }
  if (pa_mainloop_run(m, &status) < 0) {
    error("pa_mainloop_run failed");
  }
quit:
  if (context != NULL) {
    pa_context_unref(context);
  }
  if (m != NULL) {
    pa_mainloop_free(m);
  }
  return status;
}

int main(int argc, char *argv[]) {
  bool microphone = false;

  if (argc == 2 && strcmp(argv[1], "-m") == 0) {
    microphone = true;
  } else if (argc != 1) {
    fputs("usage: mute [-m]\n", stderr);
    return EXIT_FAILURE;
  }
  return toggle(microphone);
}

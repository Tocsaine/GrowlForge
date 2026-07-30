#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#define CLAP_EXPORT __declspec(dllexport)
#else
#define CLAP_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clap_version { uint32_t major, minor, revision; } clap_version_t;
#define CLAP_VERSION_MAJOR 1
#define CLAP_VERSION_MINOR 2
#define CLAP_VERSION_REVISION 0
#ifdef __cplusplus
#define CLAP_VERSION clap_version_t{CLAP_VERSION_MAJOR,CLAP_VERSION_MINOR,CLAP_VERSION_REVISION}
#else
#define CLAP_VERSION ((clap_version_t){CLAP_VERSION_MAJOR,CLAP_VERSION_MINOR,CLAP_VERSION_REVISION})
#endif

typedef uint32_t clap_id;
#define CLAP_INVALID_ID UINT32_MAX
#define CLAP_NAME_SIZE 256
#define CLAP_PATH_SIZE 1024

typedef struct clap_plugin_descriptor {
  clap_version_t clap_version;
  const char *id, *name, *vendor, *url, *manual_url, *support_url, *version, *description;
  const char *const *features;
} clap_plugin_descriptor_t;

typedef struct clap_host clap_host_t;
typedef struct clap_plugin clap_plugin_t;
typedef struct clap_process clap_process_t;

typedef struct clap_event_header {
  uint32_t size, time;
  uint16_t space_id, type;
  uint32_t flags;
} clap_event_header_t;
#define CLAP_CORE_EVENT_SPACE_ID 0
#define CLAP_EVENT_IS_LIVE (1u << 0)
#define CLAP_EVENT_DONT_RECORD (1u << 1)
#define CLAP_EVENT_PARAM_VALUE 5
#define CLAP_EVENT_PARAM_GESTURE_BEGIN 7
#define CLAP_EVENT_PARAM_GESTURE_END 8

typedef struct clap_event_param_value {
  clap_event_header_t header;
  clap_id param_id;
  void *cookie;
  int32_t note_id;
  int16_t port_index, channel, key;
  double value;
} clap_event_param_value_t;

typedef struct clap_event_param_gesture {
  clap_event_header_t header;
  clap_id param_id;
} clap_event_param_gesture_t;

typedef struct clap_input_events {
  void *ctx;
  uint32_t (*size)(const struct clap_input_events *list);
  const clap_event_header_t *(*get)(const struct clap_input_events *list, uint32_t index);
} clap_input_events_t;

typedef struct clap_output_events {
  void *ctx;
  bool (*try_push)(const struct clap_output_events *list, const clap_event_header_t *event);
} clap_output_events_t;

typedef struct clap_audio_buffer {
  float **data32;
  double **data64;
  uint32_t channel_count;
  uint32_t latency;
  uint64_t constant_mask;
} clap_audio_buffer_t;

typedef int32_t clap_process_status;
#define CLAP_PROCESS_ERROR 0
#define CLAP_PROCESS_CONTINUE 1
#define CLAP_PROCESS_CONTINUE_IF_NOT_QUIET 2
#define CLAP_PROCESS_TAIL 3
#define CLAP_PROCESS_SLEEP 4

struct clap_process {
  int64_t steady_time;
  uint32_t frames_count;
  const void *transport;
  const clap_audio_buffer_t *audio_inputs;
  clap_audio_buffer_t *audio_outputs;
  uint32_t audio_inputs_count, audio_outputs_count;
  const clap_input_events_t *in_events;
  const clap_output_events_t *out_events;
};

struct clap_host {
  clap_version_t clap_version;
  void *host_data;
  const char *name, *vendor, *url, *version;
  const void *(*get_extension)(const clap_host_t *host, const char *extension_id);
  void (*request_restart)(const clap_host_t *host);
  void (*request_process)(const clap_host_t *host);
  void (*request_callback)(const clap_host_t *host);
};

struct clap_plugin {
  const clap_plugin_descriptor_t *desc;
  void *plugin_data;
  bool (*init)(const clap_plugin_t *plugin);
  void (*destroy)(const clap_plugin_t *plugin);
  bool (*activate)(const clap_plugin_t *plugin, double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count);
  void (*deactivate)(const clap_plugin_t *plugin);
  bool (*start_processing)(const clap_plugin_t *plugin);
  void (*stop_processing)(const clap_plugin_t *plugin);
  void (*reset)(const clap_plugin_t *plugin);
  clap_process_status (*process)(const clap_plugin_t *plugin, const clap_process_t *process);
  const void *(*get_extension)(const clap_plugin_t *plugin, const char *id);
  void (*on_main_thread)(const clap_plugin_t *plugin);
};

#define CLAP_PLUGIN_FACTORY_ID "clap.plugin-factory"
typedef struct clap_plugin_factory {
  uint32_t (*get_plugin_count)(const struct clap_plugin_factory *factory);
  const clap_plugin_descriptor_t *(*get_plugin_descriptor)(const struct clap_plugin_factory *factory, uint32_t index);
  const clap_plugin_t *(*create_plugin)(const struct clap_plugin_factory *factory, const clap_host_t *host, const char *plugin_id);
} clap_plugin_factory_t;

typedef struct clap_plugin_entry {
  clap_version_t clap_version;
  bool (*init)(const char *plugin_path);
  void (*deinit)(void);
  const void *(*get_factory)(const char *factory_id);
} clap_plugin_entry_t;

#define CLAP_PLUGIN_FEATURE_AUDIO_EFFECT "audio-effect"
#define CLAP_PLUGIN_FEATURE_DISTORTION "distortion"
#define CLAP_PLUGIN_FEATURE_STEREO "stereo"

#define CLAP_EXT_AUDIO_PORTS "clap.audio-ports"
#define CLAP_PORT_MONO "mono"
#define CLAP_PORT_STEREO "stereo"
#define CLAP_AUDIO_PORT_IS_MAIN (1u << 0)
typedef struct clap_audio_port_info {
  clap_id id;
  char name[CLAP_NAME_SIZE];
  uint32_t flags, channel_count;
  const char *port_type;
  clap_id in_place_pair;
} clap_audio_port_info_t;
typedef struct clap_plugin_audio_ports {
  uint32_t (*count)(const clap_plugin_t *plugin, bool is_input);
  bool (*get)(const clap_plugin_t *plugin, uint32_t index, bool is_input, clap_audio_port_info_t *info);
} clap_plugin_audio_ports_t;

#define CLAP_EXT_PARAMS "clap.params"
#define CLAP_PARAM_IS_STEPPED (1u << 0)
#define CLAP_PARAM_IS_READONLY (1u << 3)
#define CLAP_PARAM_IS_AUTOMATABLE (1u << 5)
#define CLAP_PARAM_IS_MODULATABLE (1u << 10)
typedef struct clap_param_info {
  clap_id id;
  uint32_t flags;
  void *cookie;
  char name[CLAP_NAME_SIZE];
  char module[CLAP_PATH_SIZE];
  double min_value, max_value, default_value;
} clap_param_info_t;
typedef struct clap_plugin_params {
  uint32_t (*count)(const clap_plugin_t *plugin);
  bool (*get_info)(const clap_plugin_t *plugin, uint32_t param_index, clap_param_info_t *param_info);
  bool (*get_value)(const clap_plugin_t *plugin, clap_id param_id, double *value);
  bool (*value_to_text)(const clap_plugin_t *plugin, clap_id param_id, double value, char *display, uint32_t size);
  bool (*text_to_value)(const clap_plugin_t *plugin, clap_id param_id, const char *display, double *value);
  void (*flush)(const clap_plugin_t *plugin, const clap_input_events_t *in, const clap_output_events_t *out);
} clap_plugin_params_t;
typedef struct clap_host_params {
  void (*rescan)(const clap_host_t *host, uint32_t flags);
  void (*clear)(const clap_host_t *host, clap_id param_id, uint32_t flags);
  void (*request_flush)(const clap_host_t *host);
} clap_host_params_t;

#define CLAP_EXT_STATE "clap.state"
typedef struct clap_ostream {
  void *ctx;
  int64_t (*write)(const struct clap_ostream *stream, const void *buffer, uint64_t size);
} clap_ostream_t;
typedef struct clap_istream {
  void *ctx;
  int64_t (*read)(const struct clap_istream *stream, void *buffer, uint64_t size);
} clap_istream_t;
typedef struct clap_plugin_state {
  bool (*save)(const clap_plugin_t *plugin, const clap_ostream_t *stream);
  bool (*load)(const clap_plugin_t *plugin, const clap_istream_t *stream);
} clap_plugin_state_t;

#define CLAP_EXT_GUI "clap.gui"
#define CLAP_WINDOW_API_WIN32 "win32"
#define CLAP_WINDOW_API_COCOA "cocoa"
#define CLAP_WINDOW_API_UIKIT "uikit"
#define CLAP_WINDOW_API_X11 "x11"
#define CLAP_WINDOW_API_WAYLAND "wayland"
typedef void *clap_hwnd;
typedef void *clap_nsview;
typedef void *clap_uiview;
typedef unsigned long clap_xwnd;
typedef struct clap_window {
  const char *api;
  union {
    clap_nsview cocoa;
    clap_uiview uikit;
    clap_xwnd x11;
    clap_hwnd win32;
    void *ptr;
  };
} clap_window_t;
typedef struct clap_gui_resize_hints {
  bool can_resize_horizontally;
  bool can_resize_vertically;
  bool preserve_aspect_ratio;
  uint32_t aspect_ratio_width;
  uint32_t aspect_ratio_height;
} clap_gui_resize_hints_t;
typedef struct clap_plugin_gui {
  bool (*is_api_supported)(const clap_plugin_t *plugin, const char *api, bool is_floating);
  bool (*get_preferred_api)(const clap_plugin_t *plugin, const char **api, bool *is_floating);
  bool (*create)(const clap_plugin_t *plugin, const char *api, bool is_floating);
  void (*destroy)(const clap_plugin_t *plugin);
  bool (*set_scale)(const clap_plugin_t *plugin, double scale);
  bool (*get_size)(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height);
  bool (*can_resize)(const clap_plugin_t *plugin);
  bool (*get_resize_hints)(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints);
  bool (*adjust_size)(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height);
  bool (*set_size)(const clap_plugin_t *plugin, uint32_t width, uint32_t height);
  bool (*set_parent)(const clap_plugin_t *plugin, const clap_window_t *window);
  bool (*set_transient)(const clap_plugin_t *plugin, const clap_window_t *window);
  void (*suggest_title)(const clap_plugin_t *plugin, const char *title);
  bool (*show)(const clap_plugin_t *plugin);
  bool (*hide)(const clap_plugin_t *plugin);
} clap_plugin_gui_t;

#ifdef __cplusplus
}
#endif

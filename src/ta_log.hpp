#pragma once
#include <cstdint>
#include <string>
#include <vector>

typedef struct _iobuf FILE;

typedef enum ta_log_source {
    SRC_NONE       = 0x00000000,
    SRC_SDL        = 0x00000001,
    SRC_VULKAN     = 0x00000002,
    //SRC_PLACEHOLDER = 0x00000004,
    //SRC_PLACEHOLDER = 0x00000008,
    //SRC_PLACEHOLDER = 0x00000010,
    //SRC_PLACEHOLDER = 0x00000020,
    //SRC_PLACEHOLDER = 0x00000040,
    //SRC_PLACEHOLDER = 0x00000080,
    //SRC_PLACEHOLDER = 0x00000100,
    //SRC_PLACEHOLDER = 0x00000200,
    //SRC_PLACEHOLDER = 0x00000400,
    //SRC_PLACEHOLDER = 0x00000800,
    //SRC_PLACEHOLDER = 0x00001000,
    //SRC_PLACEHOLDER = 0x00002000,
    //SRC_PLACEHOLDER = 0x00004000,
    //SRC_PLACEHOLDER = 0x00008000,
    //SRC_PLACEHOLDER = 0x00010000,
    //SRC_PLACEHOLDER = 0x00020000,
    //SRC_PLACEHOLDER = 0x00040000,
    //SRC_PLACEHOLDER = 0x00080000,
    //SRC_PLACEHOLDER = 0x00100000,
    //SRC_PLACEHOLDER = 0x00200000,
    //SRC_PLACEHOLDER = 0x00400000,
    //SRC_PLACEHOLDER = 0x00800000,
    //SRC_PLACEHOLDER = 0x01000000,

    //...

    SRC_DEBUG      = 0x40000000,
    SRC_ALL        = 0x7fffffff
    //SRC_ALL = SRC_ASSERT | SRC_AUDIO | SRC_CONSOLE | SRC_DML | SRC_EDITOR | SRC_EVENT | SRC_FILE | SRC_FONT | SRC_GAME
    //        | SRC_GLTF | SRC_JSON | SRC_KEYBIND | SRC_LIGHT| SRC_MATH | SRC_OGX | SRC_OPENGL | SRC_PRIMITIVE
    //        | SRC_RENDER | SRC_RIGID_BODY | SRC_SCENE | SRC_SHADER | SRC_SYSTEM | SRC_TEXTURE | SRC_WINDOW
} ta_log_source;

// TODO: Actually use this
typedef enum ta_log_level {
    LEVEL_NONE  = 0x00000000,
    LEVEL_DEBUG = 0x00000001,
    LEVEL_INFO  = 0x00000002,
    LEVEL_WARN  = 0x00000004,
    LEVEL_ERROR = 0x00000008,
    LEVEL_FATAL = 0x00000010,
} ta_log_level;

// NOTE: I don't expect lock/unlock to ever error, but if it's a real thing that
// happens I'll refactor this to handle it better.
//#define TA_LOCK(mutex) assert(!SDL_LockMutex(mutex))
//#define TA_UNLOCK(mutex) assert(!SDL_UnlockMutex(mutex))
#define TA_LOCK(mutex)
#define TA_UNLOCK(mutex)
#define MAX_THREADS 8

//typedef SDL_threadID ta_thread_id;
typedef uint32_t ta_thread_id;

typedef struct ta_log_timed_region {
    std::string name;
    ta_log_source src;
    double start_ms;
} ta_log_timed_region;

typedef struct ta_log_thread_state {
    ta_thread_id thread_id;
    int indent;
    std::vector<ta_log_timed_region> timed_regions;
} ta_log_thread_state;

typedef struct ta_log {
    std::string filename;        // relative path to log file
    FILE        *stream;          // file stream to write to
    bool        flush;            // if true, flush log after every write (also flushed stdout when echo = true)
    bool        echo_stdout;      // if true, echo all log writes to stdout
    uint32_t    src_include;      // log source bitmap, 1 = log this source
    uint32_t    src_exclude;      // log source bitmap, 1 = exclude this source (overrides include)
    uint32_t    level_filter;     // TODO(unused): log level filter
    void        *mutex;           // TODO(unused): was SDL_mutex when I was testing threading
    bool        show_timestamps;  // if true, write timestamps before each line
    ta_log_thread_state thread_states[MAX_THREADS];
} ta_log;

extern ta_log tg_debug_log;

const char *ta_log_source_str(ta_log_source src);

void ta_log_init                (ta_log &log, FILE *stream, bool flush, bool echo_stdout, bool echo_console, uint32_t src_include, uint32_t src_exclude);
void ta_log_init_file           (ta_log &log, std::string filename, bool flush, bool echo_stdout, uint32_t src_include, uint32_t src_exclude);
void ta_log_flush               (ta_log &log);
void ta_log_indent              (ta_log &log);
void ta_log_unindent            (ta_log &log);
void ta_log_write               (ta_log &log, ta_log_source src, const char *fmt, ...);
void ta_log_timed_region_start  (ta_log &log, ta_log_source src, const char *name, size_t name_len);
void ta_log_timed_region_end    (ta_log &log, const char *name, size_t name_len);
void ta_log_free                (ta_log &log);
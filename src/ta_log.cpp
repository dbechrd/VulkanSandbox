#include "ta_log.hpp"
#include "ta_timer.hpp"
#include <cassert>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <ctime>

#define TA_LOG_MAX_LINE_LENGTH 1024

ta_log tg_debug_log;

const char *ta_log_source_str(ta_log_source src) {
    switch(src) {
        case SRC_SDL:       return "SDL";
        case SRC_VULKAN:    return "Vulkan";
        case SRC_DEBUG:     return "Debug";
        default:            return "UNKNOWN";
    }
}

void ta_log_init(ta_log &log, FILE *stream, bool flush, bool echo_stdout, uint32_t src_include,
    uint32_t src_exclude)
{
    assert(stream);
    log.stream = stream;
    log.flush = flush;
    log.echo_stdout = echo_stdout;
    log.src_include = src_include;
    log.src_exclude = src_exclude;
    //log.mutex = SDL_CreateMutex();
    //assert(log.mutex);
    log.show_timestamps = true;

    TA_LOCK(log.mutex);
    fprintf(log.stream,
        "[Timestamp          ][TID  ][Source    ][Elapsed  ][Message                   ]\n"
        "-------------------------------------------------------------------------------\n");
    TA_UNLOCK(log.mutex);
}

void ta_log_init_file(ta_log &log, std::string filename, bool flush, bool echo_stdout, uint32_t src_include,
    uint32_t src_exclude)
{
    FILE *stream = fopen(filename.c_str(), "wb");
    ta_log_init(log, stream, flush, echo_stdout, src_include, src_exclude);
    log.filename = filename;
}

void ta_log_flush(ta_log &log)
{
    fflush(log.stream);
    if (log.echo_stdout) {
        fflush(stdout);
    }
}

static ta_log_thread_state *log_get_or_create_thread_state(ta_log &log, ta_thread_id thread_id)
{
    //assert(thread_id);

    ta_log_thread_state *state = 0;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (log.thread_states[i].thread_id == thread_id) {
            state = &log.thread_states[i];
            break;
        } else if (!log.thread_states[i].thread_id) {
            state = &log.thread_states[i];
            state->thread_id = thread_id;
            break;
        }
    }
    if (!state) {
        assert(!"Thread table is full. Do clean-up or increase MAX_THREADS");
    }
    return state;
}

static ta_log_thread_state *log_get_thread_state(ta_log &log, ta_thread_id thread_id)
{
    ta_log_thread_state *state = 0;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (log.thread_states[i].thread_id == thread_id) {
            state = &log.thread_states[i];
            break;
        }
    }
    return state;
}

void ta_log_indent(ta_log &log)
{
    ta_thread_id thread_id = 0; //SDL_ThreadID();
    ta_log_thread_state *state = log_get_or_create_thread_state(log, thread_id);
    state->indent++;
}

void ta_log_unindent(ta_log &log)
{
    ta_thread_id thread_id = 0; //SDL_ThreadID();
    ta_log_thread_state *state = log_get_thread_state(log, thread_id);
    if (state && state->indent) {
        state->indent--;
    }
}

static void ta_log_timestamp(char *buf, int len)
{
    time_t ts = time(0);
    struct tm *date = localtime(&ts);
    strftime(buf, len, "%F %T", date);
}

static void ta_log_write_timestamp(ta_log &log, ta_log_source src)
{
    if (!log.show_timestamps) {
        return;
    }

    char timestamp[32] = "1970-01-01 00:00:00";
    ta_log_timestamp(timestamp, sizeof(timestamp));

    double elapsed_ms = ta_timer_elapsed_ms();
    double elapsed_sec = elapsed_ms / 1000;

    ta_thread_id thread_id = 0; //SDL_ThreadID();

    static char buffer[TA_LOG_MAX_LINE_LENGTH] = { 0 };
    int len = snprintf(buffer, sizeof(buffer), "[%s][%5u][%10s][%8.3fs] ", timestamp, thread_id, ta_log_source_str(src),
        elapsed_sec);
    assert(len <= TA_LOG_MAX_LINE_LENGTH);

    fwrite(buffer, 1, len, log.stream);
    if (log.echo_stdout) {
        fwrite(buffer, 1, len, stdout);
    }

    ta_log_thread_state *state = log_get_thread_state(log, thread_id);
    if (state) {
        for (const ta_log_timed_region& region : state->timed_regions) {
            double region_elapsed_ms = elapsed_ms - region.start_ms;

            len = snprintf(buffer, sizeof(buffer), "[%s: %7.3fms] ", region.name.c_str(), region_elapsed_ms);
            assert(len <= TA_LOG_MAX_LINE_LENGTH);

            fwrite(buffer, 1, len, log.stream);
            if (log.echo_stdout) {
                fwrite(buffer, 1, len, stdout);
            }
        }
    }
}

void ta_log_write(ta_log &log, ta_log_source src, const char *fmt, ...)
{
    if (log.src_include & src && !(log.src_exclude & src)) {
        TA_LOCK(log.mutex);
        ta_log_write_timestamp(log, src);

        ta_thread_id thread_id = 0; //SDL_ThreadID();
        ta_log_thread_state *state = log_get_thread_state(log, thread_id);
        if (state) {
            for (int i = 0; i < state->indent; ++i) {
                fprintf(log.stream, "    ");
            }
        }

        va_list args = {};
        va_start(args, fmt);
        vfprintf(log.stream, fmt, args);
        if (log.echo_stdout) {
            vfprintf(stdout, fmt, args);
        }
        va_end(args);

        if (log.flush) {
            ta_log_flush(log);
        }
        TA_UNLOCK(log.mutex);
    }
}

// NOTE: Lifetime of name must be at least as long as it takes to call ta_log_timed_region_end()
void ta_log_timed_region_start(ta_log &log, ta_log_source src, std::string name)
{
    ta_thread_id thread_id = {}; //SDL_ThreadID();
    ta_log_thread_state *state = log_get_or_create_thread_state(log, thread_id);

    ta_log_timed_region region = { 0 };
    region.name = name;
    region.src = src;
    region.start_ms = ta_timer_elapsed_ms();

    state->timed_regions.push_back(region);
    ta_log_write(log, src, "START\n");
}

void ta_log_timed_region_end(ta_log &log, std::string name)
{
    ta_thread_id thread_id = {}; //SDL_ThreadID();
    ta_log_thread_state *state = log_get_or_create_thread_state(log, thread_id);

    bool found = false;
    while (!found && !state->timed_regions.empty()) {
        const ta_log_timed_region &region = state->timed_regions.back();
        ta_log_write(log, region.src, "END\n");
        found = region.name == name;
        state->timed_regions.pop_back();
    }
}

void ta_log_free(ta_log &log)
{
    ta_log_flush(log);
    if (log.filename.length()) {
        fclose(log.stream);
    }
    if (log.mutex) {
        //SDL_DestroyMutex(log.mutex);
    }
    // TODO: Flush all timed regions on log close? For now, just assume app does that correctly
    //for (int i = 0; i < MAX_THREADS; ++i) {
    //    for (const ta_log_timed_region& region : log.thread_states[i].timed_regions) {
    //    }
    //}
}
/**
 * @author kyungin.kim < myohancat@naver.com >
 * my simple event loop source code
 */
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <android/log.h>

#include "mutex.h"

#ifdef __cplusplus
extern "C"
{
#endif

static LOG_LEVEL_e gLogLevel = LOG_LEVEL_TRACE;
static bool gLogWithTime = true;

static Mutex gLock;

static const char *cur_time_str(char *buf)
{
    timeval tv;
    gettimeofday(&tv, 0);
    time_t curtime = tv.tv_sec;
    tm *t = localtime(&curtime);

    sprintf(buf, "[%02d:%02d:%02d.%03ld] ", t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);

    return buf;
}

void LOG_SetLevel(LOG_LEVEL_e eLevel)
{
    gLogLevel = eLevel;
}

LOG_LEVEL_e LOG_GetLevel()
{
    return gLogLevel;
}

void LOG_Print(int priority, const char *color, const char *fmt, ...)
{
    va_list ap;

    if (priority > gLogLevel)
        return;

    int android_prio;
    char buf[4 * 1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    switch (priority)
    {
        case LOG_LEVEL_NONE:
            android_prio = ANDROID_LOG_SILENT;
            break;
        case LOG_LEVEL_ERROR:
            android_prio = ANDROID_LOG_ERROR;
            break;
        case LOG_LEVEL_WARN:
            android_prio = ANDROID_LOG_WARN;
            break;
        case LOG_LEVEL_INFO:
            android_prio = ANDROID_LOG_INFO;
            break;
        case LOG_LEVEL_DEBUG:
            android_prio = ANDROID_LOG_DEBUG;
            break;
        case LOG_LEVEL_TRACE:
            android_prio = ANDROID_LOG_VERBOSE;
            break;
        default:
            android_prio = ANDROID_LOG_UNKNOWN;
            break;
    }

    __android_log_write(android_prio, TAG, buf);
}

#define ISPRINTABLE(c)  (((c)>=32 && (c)<=126))
void LOG_Dump(int priority, const void* ptr, int size)
{
    FILE* fp;
    char buffer[1024];
    int ii, offset;
    const unsigned char* data = (const unsigned char*)ptr;

    if(priority > gLogLevel)
        return;

    gLock.lock();

    for(offset = 0; offset < size; offset += 16)
    {
        char* p = buffer;
        int remain = size - offset;

        /* print address */
        p += sprintf(p, "0x%04x  ", offset);
        if(remain > 16)
            remain = 16;

        /* print hexa value */
        for(ii = 0; ii < 16; ii++)
        {
            if(ii == 8) *p++ = ' ';

            if(offset + ii < size)
                p += sprintf(p, "%02x ", data[offset + ii]);
            else
                p += sprintf(p, "   ");
        }
        *p++ = ' ';

        /* print ascii */
        for(ii = 0; ii < remain; ii++)
        {
            if(ISPRINTABLE(data[offset + ii]))
                sprintf(p++, "%c", data[offset + ii]);
            else
                strcpy(p++, ".");
        }

        strcpy(p, "\n");
        LOG_Print(priority, "%s", buffer);
    }

    gLock.unlock();
}

#ifdef __cplusplus
} // extern "C"
#endif
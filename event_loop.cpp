/**
 * @author kyungin.kim < myohancat@naver.com >
 * my simple event loop source code
 */
#include "event_loop.h"

#include "sys_time.h"
#include "log.h"

#include <algorithm>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <inttypes.h>

static bool timer_sort(const Timer* first, const Timer* second)
{
    return first->getExpiry() < second->getExpiry();
}

EventLoop& EventLoop::getInstance()
{
    static EventLoop instance;

    return instance;
}

void EventLoop::addFdWatcher(IFdWatcher* watcher)
{
    Lock lock(mFdWatcherLock);

    if(!watcher)
        return;

    FdWatcherList::iterator it = std::find(mFdWatchers.begin(), mFdWatchers.end(), watcher);
    if(watcher == *it)
    {
        LOG_ERROR("FdWatcher is alreay exsit !!\n");
    }

    mFdWatchers.push_back(watcher);
}

void EventLoop::removeFdWatcher(IFdWatcher* watcher)
{
    Lock lock(mFdWatcherLock);

    if(!watcher)
        return;

    for(FdWatcherList::iterator it = mFdWatchers.begin(); it != mFdWatchers.end(); it++)
    {
        if(watcher == *it)
        {
            mFdWatchers.erase(it);
            return;
        }
    }
}

void EventLoop::addTimer(Timer* timer)
{
    Lock lock(mTimerLock);

    if(!timer)
        return;

    TimerList::iterator it = std::find(mTimers.begin(), mTimers.end(), timer);
    if(timer == *it)
    {
        LOG_ERROR("timer is alreay exsit !!\n");
    }

    mTimers.push_back(timer);
    mTimers.sort(timer_sort);
}

void EventLoop::removeTimer(Timer* timer)
{
    Lock lock(mTimerLock);

    if(!timer)
        return;

    for(TimerList::iterator it = mTimers.begin(); it != mTimers.end(); it++)
    {
        if(timer == *it)
        {
            mTimers.erase(it);
            mTimers.sort(timer_sort);
            return;
        }
    }
}

#define WAIT_TIME (10* 1000)

uint32_t EventLoop::runTimers()
{
    Lock lock(mTimerLock);

    int32_t timeout = WAIT_TIME;

    if(mTimers.size())
    {
        uint64_t now    = SysTime::getTickCountMs();
        uint64_t expiry = mTimers.front()->getExpiry();
        if(expiry <= now)
        {
            Timer* timer = mTimers.front();
            if(!mTimers.front()->execute())
            {
                if(timer == mTimers.front())
                    mTimers.pop_front();
            }

            mTimers.sort(timer_sort);
            if(mTimers.size())
                timeout = mTimers.front()->getExpiry() - now;    
        }
        else
            timeout = expiry - now;
    }

    if(timeout < 0)
        timeout = 0;
    else if(timeout > WAIT_TIME)
        timeout = WAIT_TIME;

    return timeout;
}

#define _MAX(x, y)   ((x>y)?x:y)
bool EventLoop::loop()
{
    int      nCnt = 0;
    int      nLastFd = -1;
    uint64_t timeToWait;
    struct   timeval sWait;

    fd_set sReadFds;
    FD_ZERO(&sReadFds);

    nLastFd = mPipe[0];
    FD_SET(mPipe[0], &sReadFds);
    
    for(FdWatcherList::iterator it = mFdWatchers.begin(); it != mFdWatchers.end(); it++)
    {
        nLastFd = _MAX((*it)->getFD(), nLastFd);
        FD_SET((*it)->getFD(), &sReadFds);
    }

    if(nLastFd < 0)
        return false;

    nLastFd++;

    while((timeToWait = runTimers()) == 0) { } // Execute Timers
        
    sWait.tv_sec  = timeToWait / 1000;
    sWait.tv_usec = (timeToWait % 1000) * 1000;

    nCnt = select(nLastFd, &sReadFds, NULL, NULL, &sWait);

    if(nCnt == 0)
    {
        /* TIMEOUT OCCURED */
        return true;
    }

    if(nCnt == -1)
    {
        if(errno == EINTR)
        {
            /* INTERRUPT RECIEVED */
            return true;
        }
        LOG_ERROR("select error oucced!!! errno=%d\n", errno);
        return false;
    }
    
    if(FD_ISSET(mPipe[0], &sReadFds))
    {
        char cCmd;
        if(read(mPipe[0], &cCmd, 1) < 0)
        {
            LOG_ERROR("pipe read failed !\n");
            return true;
        }

        switch(cCmd)
        {
            case 'T':
                LOG_INFO(">>>>> terminated recieved !\n");
                return false;
            case 'S':
                return true;
            default:
                LOG_ERROR("Unkown Thread Command Inputed '%c'!!!\n", cCmd);
                return true;
        }
    }

    mFdWatcherLock.lock();
    for(FdWatcherList::iterator it = mFdWatchers.begin(); it != mFdWatchers.end(); it++)
    {
        if(FD_ISSET((*it)->getFD(), &sReadFds))
        {
            (*it)->onFdReadable((*it)->getFD());
        }
    }
    mFdWatcherLock.unlock();

    return true;
}

void EventLoop::wakeup()
{
    if(mPipe[1] >= 0)
        write(mPipe[1], "S", 1);
}

void EventLoop::terminate()
{
    if(mPipe[1] >= 0)
        write(mPipe[1], "T", 1);
}

EventLoop::EventLoop()
{
    /* for stop command */
    if(pipe(mPipe) < 0)
    {
        LOG_ERROR("cannot create pipe !\n");
    }
}

EventLoop::~EventLoop()
{
    if(mPipe[0] >= 0)
        close(mPipe[0]);

    if(mPipe[1] >= 0)
        close(mPipe[1]);
}


/*
 *  Some functions are possibly missing on MacOS and in this case
 *  are replaced with "static inline" functions:
 *
 *  clock_gettime(void)
 *  clock_nanosleep(void)
 */

#ifdef __APPLE__

#include <time.h>

#if !defined(CLOCK_REALTIME) && !defined(CLOCK_MONOTONIC)
//
// MacOS < 10.12 does not have clock_gettime
//
// Contributed initially by Davide "ra1nb0w"
//

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 6
#define CLOCK_MONOTONIC_RAW 4
typedef int clockid_t;

#include <sys/time.h>
#include <mach/mach_time.h>

// here to avoid problem on linking
static inline int clock_gettime( clockid_t clk_id, struct timespec *ts )
{
  int ret = -1;
  if ( ts )
  {
    if      ( CLOCK_REALTIME == clk_id )
    {
      struct timeval tv;
      ret = gettimeofday(&tv, NULL);
      ts->tv_sec  = tv.tv_sec;
      ts->tv_nsec = tv.tv_usec * 1000;
    }
    else if ( CLOCK_MONOTONIC == clk_id  || CLOCK_MONOTONIC_RAW == clk_id )
    {
      //
      // For the time being, accept CLOCK_MONOTONIC_RAW but treat it
      // the same way as CLOCK_MONOTONIC.
      //
      const uint64_t t = mach_absolute_time();
      mach_timebase_info_data_t timebase;
      mach_timebase_info(&timebase);
      const uint64_t tdiff = t * timebase.numer / timebase.denom;
      ts->tv_sec  = tdiff / 1000000000;
      ts->tv_nsec = tdiff % 1000000000;
      ret = 0;
    }
  }
  return ret;
}

#endif // CLOCK_REALTIME and CLOCK_MONOTONIC

//
// MacOS does not have clock_nanosleep but it does have nanosleep
// We ignore clock_id (assuming CLOCK_MONOTONIC)
// but for the flags we allow TIMER_ABSTIME (sleep until a specific poin
// in time), for all other value we sleep for a speficic period.
//

#if !defined(TIMER_ABSTIME)
#define TIMER_ABSTIME 12345

static inline int clock_nanosleep(clockid_t clock_id, int flags,
                              const struct timespec *request,
                              struct timespec *remain) {
  struct timespec now;
  int rc;

  if (flags == TIMER_ABSTIME) {
    //
    // sleep until point in the future
    //
    clock_gettime(CLOCK_MONOTONIC, &now);
    now.tv_sec = request->tv_sec  - now.tv_sec;
    now.tv_nsec= request->tv_nsec - now.tv_nsec;
    while (now.tv_nsec < 0) {
      now.tv_nsec += 1000000000;
      now.tv_sec--;
    }
    rc=nanosleep(&now, remain);
  } else {
    //
    // sleep for the given period
    //
    rc=nanosleep(request, remain);
  }
  return rc;
}
#endif  // !defined(TIMER_ABSTIME)

#endif // __APPLE__

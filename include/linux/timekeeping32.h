#ifndef _LINUX_TIMEKEEPING32_H
#define _LINUX_TIMEKEEPING32_H
/*
 * These interfaces are all based on the old timespec type
 * and should get replaced with the timespec64 based versions
 * over time so we can remove the file here.
 */

static inline unsigned long get_seconds(void)
{
	return ktime_get_real_seconds();
}

/* Map the ktime_t to timespec conversion to ns_to_timespec function */
#define ktime_to_timespec(kt)		ns_to_timespec((kt))
#define timespec			old_timespec32

static inline struct timespec ns_to_timespec(const s64 nsec)
{
	struct timespec ts;
	s32 rem;
	if (!nsec)
		return (struct timespec) {0, 0};
	ts.tv_sec = div_s64_rem(nsec, NSEC_PER_SEC, &rem);
	if (unlikely(rem < 0)) {
		ts.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts.tv_nsec = rem;
	return ts;
}

/* timespec64 is defined as timespec here */
static inline struct timespec timespec64_to_timespec(const struct timespec64 ts64)
{
	return *(const struct timespec *)&ts64;
}

static inline void getboottime(struct timespec *ts)
{
	struct timespec64 ts64;

	getboottime64(&ts64);
	*ts = timespec64_to_timespec(ts64);
}

static inline void get_monotonic_boottime(struct timespec *ts)
{
	*ts = ktime_to_timespec(ktime_get_boottime());
}

static inline void timekeeping_clocktai(struct timespec *ts)
{
	*ts = ktime_to_timespec(ktime_get_clocktai());
}


#endif

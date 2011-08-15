#ifndef _AWF_PROFILE_HEADER
#define _AWF_PROFILE_HEADER

#define THREAD __thread

#ifndef _PROFILE
#define _PROFILE 0
#endif

#ifndef _PROFILE_PAPI_EVENTS
#define _PROFILE_PAPI_EVENTS 0
#endif

#ifndef _PROFILE_PAPI_FILE
#define _PROFILE_PAPI_FILE 0
#endif

#ifndef _PROFILE_MPI
#define _PROFILE_MPI 0
#endif

#ifndef _PROFILE_HMPI
#define _PROFILE_HMPI 0
#endif

#if _PROFILE == 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <papi.h>


#if _PROFILE_PAPI_EVENTS == 1

#define NUM_EVENTS 4

static int _profile_events[NUM_EVENTS] =
        //{ PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_L2_TCM, PAPI_HW_INT };
        { PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_L2_TCM, PAPI_RES_STL };
//        { PAPI_TOT_CYC, PAPI_TOT_INS, 1073741862, 1073741935 };
        //{ PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_HW_INT, 1073741935 };

extern THREAD FILE* _profile_fd;

#if _PROFILE_PAPI_FILE == 1
#define PROFILE_DECLARE() \
  THREAD FILE* _profile_fd;
  ///*__thread*/ struct profile_info_t _profile_info; 

#else
#define PROFILE_DECLARE()

#endif

#else

#define PROFILE_DECLARE()
  ///*__thread*/ struct profile_info_t _profile_info; 

#endif


typedef struct profile_vars_t {
    uint64_t time;
    uint64_t count;
    uint64_t start;
#if _PROFILE_PAPI_EVENTS == 1
    uint64_t ctrs[NUM_EVENTS];
#endif
} profile_vars_t;


#define PROFILE_VAR(v) \
    THREAD profile_vars_t _profile_ ## v = {0};

#define PROFILE_EXTERN(v) \
    extern THREAD profile_vars_t _profile_ ## v;


//This needs to be declared once in a C file somewhere
//extern /*__thread*/ struct profile_info_t _profile_info;

static inline void PROFILE_INIT(int tid)
{
  if(tid == 0) {
    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if(ret < 0) {
        printf("PAPI init failure\n");
        fflush(stdout);
        exit(-1);
    }

    PAPI_thread_init(pthread_self);

#if _PROFILE_PAPI_EVENTS == 1
    int num_hwcntrs = 0;

    if ((num_hwcntrs = PAPI_num_counters()) <= PAPI_OK) {
        printf("ERROR PAPI_num_counters\n");
        exit(-1);
    }

    if(num_hwcntrs < NUM_EVENTS) {
        printf("ERROR PAPI reported < %d events available\n", NUM_EVENTS);
    }
#endif
  }
#if _PROFILE_PAPI_EVENTS == 1
#if _PROFILE_PAPI_FILE == 1
    char filename[128];

    sprintf(filename, "profile-%d.out", tid);
    _profile_fd = fopen(filename, "w+");
    if(_profile_fd == NULL) {
        printf("ERROR opening profile data file\n");
        exit(-1);
    }

    fprintf(_profile_fd, "VAR TIME");
    for(int i = 0; i < NUM_EVENTS; i++) {
        PAPI_event_info_t info;
        if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) {
            printf("ERROR PAPI_get_event_info %d\n", i);
            continue;
        }

        printf("PAPI event %16s %s\n", info.symbol, info.long_descr);
        fflush(stdout);

        fprintf(_profile_fd, " %s", info.symbol);
    }
    fprintf(_profile_fd, "\n");
#endif
#endif
}


static inline void PROFILE_FINALIZE()
{
#if _PROFILE_PAPI_EVENTS == 1
#if _PROFILE_PAPI_FILE == 1
    fclose(_profile_fd);
#endif
#endif
}


#define PROFILE_START(v) __PROFILE_START(&(_profile_ ## v))

static inline void __PROFILE_START(struct profile_vars_t* v)
{
#if _PROFILE_PAPI_EVENTS == 1
    PAPI_start_counters(_profile_events, NUM_EVENTS);
#endif
    //v->start = PAPI_get_real_usec();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    v->start = ((uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec) / 1000;
}


#define PROFILE_STOP(v) __PROFILE_STOP(#v, &_profile_ ## v)

static inline void __PROFILE_STOP(char* name, struct profile_vars_t* v)
{
    //Grab the time right away
    //uint64_t t = PAPI_get_real_usec() - v->start;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t t = (((uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec) / 1000) - v->start;

#if _PROFILE_PAPI_EVENTS == 1
    //Grab counter values
    uint64_t ctrs[NUM_EVENTS] = {0};
    PAPI_read_counters((long long*)ctrs, NUM_EVENTS);
#endif

    //Accumulate the time
    v->time += t;
    v->count++;

#if _PROFILE_PAPI_EVENTS == 1
    //Accumulate the counter values
    for(int i = 0; i < NUM_EVENTS; i++) {
        v->ctrs[i] += ctrs[i];
    }

#if _PROFILE_PAPI_FILE == 1
    fprintf(_profile_fd, "%s %lu", name, t);
    for(int i = 0; i < NUM_EVENTS; i++) {
        fprintf(_profile_fd, " %lu", ctrs[i]);
    }

    fprintf(_profile_fd, "\n");
#endif
#endif
}


#define PROFILE_SHOW(v) __PROFILE_SHOW(#v, &_profile_ ## v)

static void __PROFILE_SHOW(char* name, struct profile_vars_t* v)
{
    printf("%12s cnt %-7lu time %lu us total %08.3lf avg\n",
            name, v->count, v->time, (double)v->time / v->count);

#if _PROFILE_PAPI_EVENTS == 1
    for(int i = 0; i < NUM_EVENTS; i++) {
        PAPI_event_info_t info;
        if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) {
            printf("ERROR PAPI_get_event_info %d\n", i);
            continue;
        }

        printf("    %20s %lu total %8.3lf avg\n", info.symbol,
                v->ctrs[i], (double)v->ctrs[i] / v->count);
    }
#endif
}


#if _PROFILE_MPI == 1
#include <mpi.h>

#define PROFILE_SHOW_REDUCE(v) __PROFILE_SHOW_REDUCE(#v, &_profile_ ## v)

static void __PROFILE_SHOW_REDUCE(char* name, struct profile_vars_t* v)
{
    uint64_t rt;
    double ra;

    double a = (double)v->time / v->count;

    MPI_Reduce(&v->time, &rt, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&a, &ra, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#if _PROFILE_PAPI_EVENTS == 1
    uint64_t rtc[NUM_EVENTS];
    double rac[NUM_EVENTS];
    double ac[NUM_EVENTS];

    for(int i = 0; i < NUM_EVENTS; i++) {
        //printf("%d %d ctr %lu\n", rank, i, v->ctrs[i]); fflush(stdout);
        ac[i] = (double)v->ctrs[i] / v->count;
    }

    MPI_Reduce(v->ctrs, rtc, NUM_EVENTS,
            MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&ac, rac, NUM_EVENTS,
            MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

#endif


    if(rank == 0) {
        printf("TIME %12s cnt %-7lu time %lf us total %8.3lf avg\n", name,
                v->count, (double)v->time / 1000.0, (double)v->time / v->count);

#if _PROFILE_PAPI_EVENTS == 1
        for(int i = 0; i < NUM_EVENTS; i++) {
            PAPI_event_info_t info;
            if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) {
                printf("ERROR PAPI_get_event_info %d\n", i);
                continue;
            }

            printf("PAPI %20s %lu total %8.3lf avg\n", info.symbol, rtc[i], rac[i]);
        }
#endif
    }
}

#elif _PROFILE_HMPI == 1
#include "hmpi.h"

#define PROFILE_SHOW_REDUCE(v) __PROFILE_SHOW_REDUCE(#v, &_profile_ ## v)

static void __PROFILE_SHOW_REDUCE(char* name, struct profile_vars_t* v)
{
    uint64_t rt;
    double ra;

    double a = (double)v->time / v->count;

    HMPI_Allreduce(&v->time, &rt, 1, MPI_LONG_LONG, MPI_SUM, HMPI_COMM_WORLD);
    HMPI_Allreduce(&a, &ra, 1, MPI_DOUBLE, MPI_SUM, HMPI_COMM_WORLD);

#if _PROFILE_PAPI_EVENTS == 1
    uint64_t rtc[NUM_EVENTS];
    double rac[NUM_EVENTS];
    double ac[NUM_EVENTS];

    for(int i = 0; i < NUM_EVENTS; i++) {
        ac[i] = (double)v->ctrs[i] / v->count;
    }

    HMPI_Allreduce(v->ctrs, rtc, NUM_EVENTS,
            MPI_LONG_LONG, MPI_SUM, HMPI_COMM_WORLD);
    HMPI_Allreduce(&ac, rac, NUM_EVENTS,
            MPI_DOUBLE, MPI_SUM, HMPI_COMM_WORLD);

#endif

    int r;
    HMPI_Comm_rank(HMPI_COMM_WORLD, &r);

    if(r == 0) {
        printf("TIME %12s cnt %-7lu time %lf us total %8.3lf avg\n", name,
                v->count, (double)v->time / 1000.0, (double)v->time / v->count);

        for(int i = 0; i < NUM_EVENTS; i++) {
            PAPI_event_info_t info;
            if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) {
                printf("ERROR PAPI_get_event_info %d\n", i);
                continue;
            }

            printf("PAPI %20s %lu total %8.3lf avg\n", info.symbol, rtc[i], rac[i]);
        }
    }
}

#else
#define PROFILE_SHOW_REDUCE(var)
#endif


#warning "PROFILING ON"

#else
#define PROFILE_DECLARE()
static inline void PROFILE_INIT(int tid) {}
static inline void PROFILE_FINALIZE(void) {}
#define PROFILE_VAR(var)
#define PROFILE_EXTERN(var)
#define PROFILE_START(var)
#define PROFILE_STOP(var)
#define PROFILE_SHOW(var)
#define PROFILE_SHOW_REDUCE(var)
//#warning "PROFILING OFF"
#endif

#endif

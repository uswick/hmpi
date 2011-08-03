#ifndef _AWF_PROFILE_HEADER
#define _AWF_PROFILE_HEADER

//#define _PROFILE 1

#ifdef _PROFILE

#include <stdio.h>
#include <string.h>
#include <papi.h>


#define NUM_EVENTS 2
#define NUM_RECORDS 1024 //Number of records to store before flushing to disk


static int _profile_events[NUM_EVENTS] =
        //{ PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_L2_TCM, PAPI_HW_INT };
        { PAPI_TOT_CYC, PAPI_TOT_INS };


#define PROFILE_VAR(var) \
    uint64_t time_ ## var; \
    uint64_t count_ ## var; \
    uint64_t tmp_ ## var; \
    uint64_t ctrs_ ## var[NUM_EVENTS]; \
    uint64_t rec_ ## var[NUM_RECORDS][NUM_EVENTS + 1];

typedef struct profile_vars_t {
    uint64_t time;
    uint64_t count;
    uint64_t start;
    uint64_t ctrs[NUM_EVENTS];
    //uint64_t rec[NUM_RECORDS][NUM_EVENTS + 1]; //Last event is time
} profile_vars_t;


struct profile_info_t {
/*    PROFILE_VAR(match);
    PROFILE_VAR(copy);
    PROFILE_VAR(send);
    PROFILE_VAR(add_send_req);
    PROFILE_VAR(barrier);
    PROFILE_VAR(alltoall);*/
    PROFILE_VAR(memcpy);
};



//This needs to be declared once in a C file somewhere
extern /*__thread*/ struct profile_info_t _profile_info;
extern FILE* _profile_fd;

#define PROFILE_DECLARE() \
  /*__thread*/ struct profile_info_t _profile_info; \
  FILE* _profile_fd;

static inline void PROFILE_INIT(void)
{
    int num_hwcntrs = 0;
    int i;

    memset(&_profile_info, 0, sizeof(struct profile_info_t));

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if(ret < 0) {
        printf("PAPI init failure\n");
        fflush(stdout);
        exit(-1);
    }

    if ((num_hwcntrs = PAPI_num_counters()) <= PAPI_OK) {
        printf("ERROR PAPI_num_counters\n");
        exit(-1);
    }

    if(num_hwcntrs < NUM_EVENTS) {
        printf("ERROR PAPI reported < %d events available\n", NUM_EVENTS);
    }

    _profile_fd = fopen("profile.out", "w+");
    if(_profile_fd == NULL) {
        printf("ERROR opening profile data file\n");
        exit(-1);
    }

    fprintf(_profile_fd, "VAR TIME");
    for(i = 0; i < NUM_EVENTS; i++) {
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
}


static inline void PROFILE_FINALIZE()
{
    fclose(_profile_fd);
}


#if 0
#define PROFILE_START(var) { \
    PAPI_start_counters(_profile_events, NUM_EVENTS); \
    _profile_info.tmp_ ## var = PAPI_get_real_usec(); \
}
#endif

static inline void PROFILE_START(struct profile_vars_t* v)
{
    PAPI_start_counters(_profile_events, NUM_EVENTS);
    v->start = PAPI_get_real_cyc();
}


#if 0
#define PROFILE_STOP(var) { \
    uint64_t t = PAPI_get_real_usec() - _profile_info.tmp_ ## var; \
    int ind = _profile_info.count_ ## var % NUM_RECORDS; \
    /*PAPI_accum_counters((long long)_profile_info.ctrs_ ## var, NUM_EVENTS); */\
    PAPI_read_counters((long long)_profile_info.rec_ ## var[ind], NUM_EVENTS); \
    _profile_info.time_ ## var += t; \
    _profile_info.rec_ ## var[ind] += t; \
    for(int i = 0; i < NUM_EVENTS; i++) \
        _profile_info.ctrs_ ## var[i] += _profile_info.rec_ ## var[ind][i]; \
    _profile_info.count_ ## var ++; \
}
#endif


#define PROFILE_STOP(v) __PROFILE_STOP(#v, v)

static inline void __PROFILE_STOP(char* name, struct profile_vars_t* v)
{
    //Grab the time right away
    uint64_t t = PAPI_get_real_cyc() - v->start;

    //Grab counter values
    uint64_t ctrs[NUM_EVENTS];
    PAPI_read_counters((long long*)ctrs, NUM_EVENTS);

    //Accumulate the time
    v->time += t;
    v->count++;

    //Accumulate the counter values
    fprintf(_profile_fd, "%s %lu", name, t);
    for(int i = 0; i < NUM_EVENTS; i++) {
        v->ctrs[i] += ctrs[i];
        fprintf(_profile_fd, " %lu", ctrs[i]);
    }

    fprintf(_profile_fd, "\n");
}


#if 0
#define PROFILE_SHOW(var) { \
    printf("%12s cnt %-7lu time %-10.3lf us total %08.3lf avg\n", \
            #var, _profile_info.count_ ## var, \
            (double)_profile_info.time_ ## var, \
            (double)_profile_info.time_ ## var / _profile_info.count_ ## var); \
    int i; \
    for(i = 0; i < NUM_EVENTS; i++) { \
        PAPI_event_info_t info; \
        if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) { \
            printf("ERROR PAPI_get_event_info %d\n", i); \
            continue; \
        } \
 \
        printf("    %20s %lu total %8.3lf avg\n", info.symbol, \
                _profile_info.ctrs_ ## var[i], \
                (double)_profile_info.ctrs_ ## var[i] / _profile_info.count_ ## var); \
    } \
}
#endif

#define PROFILE_SHOW(v) __PROFILE_SHOW(#v, v)

static inline void __PROFILE_SHOW(char* name, struct profile_vars_t* v)
{
    printf("%12s cnt %-7lu time %lu us total %08.3lf avg\n",
            name, v->count, v->time, (double)v->time / v->count);

    for(int i = 0; i < NUM_EVENTS; i++) {
        PAPI_event_info_t info;
        if(PAPI_get_event_info(_profile_events[i], &info) != PAPI_OK) {
            printf("ERROR PAPI_get_event_info %d\n", i);
            continue;
        }

        printf("    %20s %lu total %8.3lf avg\n", info.symbol,
                v->ctrs[i], (double)v->ctrs[i] / v->count);
    }
}


#define PROFILE_SHOW_REDUCE(var, r) { \
    double rt, ra; \
    double t = (double)_profile_info.time_ ## var; \
    double a = t / (double)_profile_info.count_ ## var; \
    HMPI_Allreduce(&t, &rt, 1, MPI_DOUBLE, MPI_SUM, HMPI_COMM_WORLD); \
    HMPI_Allreduce(&a, &ra, 1, MPI_DOUBLE, MPI_SUM, HMPI_COMM_WORLD); \
    int size; \
    HMPI_Comm_size(HMPI_COMM_WORLD, &size); \
    if(r == 0) { \
        printf("%8s cnt %-7llu time %-8.3lf us total %08.3lf avg\n", \
                #var, _profile_info.count_ ## var, \
                rt / (double)size, ra / (double)size); \
    } \
}


#warning "PROFILING ON"

#else
#define PROFILE_DECLARE()
static inline void PROFILE_INIT(void) {}
#define PROFILE_VAR(var)
#define PROFILE_START(var)
#define PROFILE_STOP(var)
#define PROFILE_SHOW(var, r)
#define PROFILE_SHOW_REDUCE(var, r)
#warning "PROFILING OFF"
#endif


#endif


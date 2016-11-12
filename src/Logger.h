#include <stdio.h>
#include <unistd.h>

#ifndef LOG_CTRL_H
#define LOG_CTRL_H
#define LOG_CTRL_CONTINUE

    #define DEBUG 1
    #define INFO  2
    #define WARN  3
    #define ERROR 4
    #define NONE  7

#endif

#ifndef AUTHER_x000032001

    extern int xxxLogLevel __attribute__((unused));

#endif


#ifdef LOG_CTRL_CONTINUE

    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define SET_LOG_LEVEL(x) (xxxDebugLevel=(x))

    #define slogf(inputLV,format,...)         \
            do { if((inputLV)>=xxxLogLevel) {     \
            fprintf(stderr,"[%5d][%.5s] %.6s:%3d %.12s() # " format,getpid(),#inputLV,__FILENAME__,__LINE__,__func__,##__VA_ARGS__);  \
            } \
            if((inputLV)>=ERROR) { exit(1); } \
            } while(0)

#endif

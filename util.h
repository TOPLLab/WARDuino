#ifndef UTIL_H
#define UTIL_H

void end();

#define FATAL(...) { \
    printf("Error(%s:%d): \n", __FILE__, __LINE__); \
    printf(__VA_ARGS__); end(); \
}

#define info(...) {\
 printf(__VA_ARGS__);\
}
#define log(...) ;
#define error(...);
#define ASSERT(exp, ...) { \
    if (! (exp)) { \
        printf("Assert Failed (%s:%d): ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); end(); \
    } \
}
#define debug(...) {\
   printf("DEBUG (%s:%d):\n ", __FILE__, __LINE__); \
   printf(__VA_ARGS__);\
}

#define warn(...) {\
   printf(__VA_ARGS__);\
}

#endif

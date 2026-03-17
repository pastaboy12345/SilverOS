#ifndef SILVEROS_V8_RUNTIME_H
#define SILVEROS_V8_RUNTIME_H

#ifdef __cplusplus
#include "v8-platform.h"
extern "C" {
#endif

// Forward declaration of V8 Platform for C
struct v8_platform;
typedef struct v8_platform v8_platform_t;

// Factory function to create the SilverOS V8 platform
void* create_silver_platform(void);

#ifdef __cplusplus
}
#endif

#endif

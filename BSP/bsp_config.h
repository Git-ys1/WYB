#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

/*
 * HAL port is enabled by default for firmware builds.
 * Toolchain preprocessor macro BSP_USE_HAL_PORT can override this value.
 */
#ifndef BSP_USE_HAL_PORT
#define BSP_USE_HAL_PORT 1
#endif

#ifndef BSP_I2C2_FAST_400K
#define BSP_I2C2_FAST_400K 0
#endif

#endif

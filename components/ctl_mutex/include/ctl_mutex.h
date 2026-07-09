#ifndef CTL_MUTEX_H
#define CTL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* 控制源 */
typedef enum {
    CTL_NONE     = 0,
    CTL_TOUCH    = 1,  /* 触控屏 */
    CTL_HTTP     = 2,  /* 网页 REST API */
    CTL_XIAOZHI  = 3,  /* 小智语音 */
} ctl_source_t;

/**
 * @brief 尝试获取控制权（非阻塞）
 * @param src  请求控制权的来源
 * @return true=成功获取, false=已被其他源占用
 */
bool ctl_try_acquire(ctl_source_t src);

/**
 * @brief 释放控制权
 * @param src  释放控制权的来源（必须与获取时一致）
 */
void ctl_release(ctl_source_t src);

/**
 * @brief 查询当前控制权归属
 * @return 当前持有控制权的来源，CTL_NONE 表示空闲
 */
ctl_source_t ctl_get_owner(void);

/**
 * @brief 查询指定来源是否持有控制权
 */
static inline bool ctl_is_owner(ctl_source_t src) {
    return ctl_get_owner() == src;
}

/**
 * @brief 查询系统是否空闲（无任何控制源占用）
 */
static inline bool ctl_is_idle(void) {
    return ctl_get_owner() == CTL_NONE;
}

#ifdef __cplusplus
}
#endif

#endif

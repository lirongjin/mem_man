#ifndef __MEM_MAN_H__
#define __MEM_MAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_container.h"
#include "linear_container.h"
#include "errno.h"

#ifndef ENOERR
#define ENOERR      0
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)     sizeof (arr) / sizeof ((arr)[0])
#endif

/*地址对齐尺寸*/
#define MEM_MAN_ALIGN_SIZE      (8)

/*内存管理数据结构*/
typedef struct _MemMan {
    LinearContainerMan lcm; /*线性容器管理器*/
    DynamicCtnMan dcm;      /*动态容器管理器*/
} MemMan;

int MMInit(MemMan *memMan, uint8_t *buf, unsigned int size);
void *MMAlloc(MemMan *memMan, size_t size);
void MMFree(MemMan *memMan, void *pointer);

#ifdef __cplusplus
}
#endif

#endif


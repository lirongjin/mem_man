#ifndef __LINEAR_CONTAINER_H__
#define __LINEAR_CONTAINER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#define CONTAINER_SIZE          3

/*线性容器元数据*/
typedef struct {
    uint8_t *base;  /*基地址*/
    uint32_t size;  /*元数据尺寸*/
} LCMCtnMeta;

/*线性容器*/
typedef struct _LCMLinearContainer {
    LCMCtnMeta meta;
    uint8_t *base;              //对齐后的基地址
    unsigned int unitSize;      //内存管理单元大小
    unsigned int unitCount;     //内存管理单元数量
} LCMLinearContainer;

/*线性容器管理器*/
typedef struct _LinearContainerMan {
    LCMLinearContainer containers[CONTAINER_SIZE];
} LinearContainerMan;

int LCMInit(LinearContainerMan *lcm, uint8_t *buf, unsigned int size, uint32_t *pRemain);
void *LCMAlloc(LinearContainerMan *lcm, size_t size);
void LCMFree(LinearContainerMan *lcm, void *pointer);

#ifdef __cplusplus
}
#endif

#endif /*__LINEAR_CONTAINER_H__*/

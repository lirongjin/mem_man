#ifndef __DYNAMIC_CONTAINER_H__
#define __DYNAMIC_CONTAINER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/*动态内存管理容器。*/
typedef struct _DCMContainer {
    unsigned int chunkCnt;      /*容器中的块数量*/
    void *prev;
    void *next;
} DCMContainer;

/* 动态内存管理数据结构。
 * 共包含32个动态容器，从容器3开始，每个容器管理一部分块。例如：容器3中管理(0-8]大小的块，
 * 容器4管理(8-16]大小的块，容器5管理(16-32]大小的块，以此类推。*/
typedef struct {
    DCMContainer containers[32];    /*容器数组*/
    uint8_t *memBase;               /*动态内存管理堆区的基地址。*/
    unsigned int memSize;           /*动态内存管理堆区大小。*/
} DynamicCtnMan;

int DCMInit(DynamicCtnMan *dcm, uint8_t *buffer, uint32_t bufLen);
void *DCMAlloc(DynamicCtnMan *dcm, size_t size);
void DCMFree(DynamicCtnMan *dcm, void *pointer);

void DCMPrint(DynamicCtnMan *dcm);

#ifdef __cplusplus
}
#endif

#endif /*__DYNAMIC_CONTAINER_H__*/



#ifndef __LINEAR_CONTAINER_H__
#define __LINEAR_CONTAINER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "linear_containers_define.h"

/*线性容器元数据*/
typedef struct {
    uint8_t *base;  /*基地址*/
    uint32_t size;  /*元数据尺寸*/
} LCMCtnMeta;

/*线性容器*/
typedef struct _LCMLinearContainer {
    LCMCtnMeta metas[2];
    uint8_t *base;              //对齐后的基地址
    unsigned int unitSize;      //内存管理单元大小
    unsigned int unitCount;     //内存管理单元数量
} LCMLinearContainer;

/*线性容器管理器*/
typedef struct _LinearContainerMan {
    LCMLinearContainer containers[CONTAINER_SIZE];
} LinearContainerMan;

int LCMInit(LinearContainerMan *lcm, uint8_t *buf, size_t size, size_t *pRemain);
void *LCMAlloc(LinearContainerMan *lcm, size_t size);
void LCMFree(LinearContainerMan *lcm, void *pointer);

void LCMPrint(LinearContainerMan *lcm);

/*
 * 功能：初始化容器的内存单元尺寸和内存单元数量
 * 返回值：
 */
static inline void LCMContainerInitUnit(LCMLinearContainer *container, size_t pos)
{
    switch (pos) {
    case 0:
        container->unitSize = CONTAINER0_UNIT_SIZE;
        container->unitCount = CONTAINER0_UNIT_COUNT;
        break;
    case 1:
        container->unitSize = CONTAINER1_UNIT_SIZE;
        container->unitCount = CONTAINER1_UNIT_COUNT;
        break;
    case 2:
        container->unitSize = CONTAINER2_UNIT_SIZE;
        container->unitCount = CONTAINER2_UNIT_COUNT;
        break;
    case 3:
        container->unitSize = CONTAINER3_UNIT_SIZE;
        container->unitCount = CONTAINER3_UNIT_COUNT;
        break;
    case 4:
        container->unitSize = CONTAINER4_UNIT_SIZE;
        container->unitCount = CONTAINER4_UNIT_COUNT;
        break;
    case 5:
        container->unitSize = CONTAINER5_UNIT_SIZE;
        container->unitCount = CONTAINER5_UNIT_COUNT;
        break;
    case 6:
        container->unitSize = CONTAINER6_UNIT_SIZE;
        container->unitCount = CONTAINER6_UNIT_COUNT;
        break;
    case 7:
        container->unitSize = CONTAINER7_UNIT_SIZE;
        container->unitCount = CONTAINER7_UNIT_COUNT;
        break;

    case 8:
        container->unitSize = CONTAINER8_UNIT_SIZE;
        container->unitCount = CONTAINER8_UNIT_COUNT;
        break;
    case 9:
        container->unitSize = CONTAINER9_UNIT_SIZE;
        container->unitCount = CONTAINER9_UNIT_COUNT;
        break;
    case 10:
        container->unitSize = CONTAINER10_UNIT_SIZE;
        container->unitCount = CONTAINER10_UNIT_COUNT;
        break;
    case 11:
        container->unitSize = CONTAINER11_UNIT_SIZE;
        container->unitCount = CONTAINER11_UNIT_COUNT;
        break;
    case 12:
        container->unitSize = CONTAINER12_UNIT_SIZE;
        container->unitCount = CONTAINER12_UNIT_COUNT;
        break;
    case 13:
        container->unitSize = CONTAINER13_UNIT_SIZE;
        container->unitCount = CONTAINER13_UNIT_COUNT;
        break;
    case 14:
        container->unitSize = CONTAINER14_UNIT_SIZE;
        container->unitCount = CONTAINER14_UNIT_COUNT;
        break;
    case 15:
        container->unitSize = CONTAINER15_UNIT_SIZE;
        container->unitCount = CONTAINER15_UNIT_COUNT;
        break;

    case 16:
        container->unitSize = CONTAINER16_UNIT_SIZE;
        container->unitCount = CONTAINER16_UNIT_COUNT;
        break;
    case 17:
        container->unitSize = CONTAINER17_UNIT_SIZE;
        container->unitCount = CONTAINER17_UNIT_COUNT;
        break;
    case 18:
        container->unitSize = CONTAINER18_UNIT_SIZE;
        container->unitCount = CONTAINER18_UNIT_COUNT;
        break;
    case 19:
        container->unitSize = CONTAINER19_UNIT_SIZE;
        container->unitCount = CONTAINER19_UNIT_COUNT;
        break;
    case 20:
        container->unitSize = CONTAINER20_UNIT_SIZE;
        container->unitCount = CONTAINER20_UNIT_COUNT;
        break;
    case 21:
        container->unitSize = CONTAINER21_UNIT_SIZE;
        container->unitCount = CONTAINER21_UNIT_COUNT;
        break;
    case 22:
        container->unitSize = CONTAINER22_UNIT_SIZE;
        container->unitCount = CONTAINER22_UNIT_COUNT;
        break;
    case 23:
        container->unitSize = CONTAINER23_UNIT_SIZE;
        container->unitCount = CONTAINER23_UNIT_COUNT;
        break;

    case 24:
        container->unitSize = CONTAINER24_UNIT_SIZE;
        container->unitCount = CONTAINER24_UNIT_COUNT;
        break;
    case 25:
        container->unitSize = CONTAINER25_UNIT_SIZE;
        container->unitCount = CONTAINER25_UNIT_COUNT;
        break;
    case 26:
        container->unitSize = CONTAINER26_UNIT_SIZE;
        container->unitCount = CONTAINER26_UNIT_COUNT;
        break;
    case 27:
        container->unitSize = CONTAINER27_UNIT_SIZE;
        container->unitCount = CONTAINER27_UNIT_COUNT;
        break;
    case 28:
        container->unitSize = CONTAINER28_UNIT_SIZE;
        container->unitCount = CONTAINER28_UNIT_COUNT;
        break;
    case 29:
        container->unitSize = CONTAINER29_UNIT_SIZE;
        container->unitCount = CONTAINER29_UNIT_COUNT;
        break;
    case 30:
        container->unitSize = CONTAINER30_UNIT_SIZE;
        container->unitCount = CONTAINER30_UNIT_COUNT;
        break;
    case 31:
        container->unitSize = CONTAINER31_UNIT_SIZE;
        container->unitCount = CONTAINER31_UNIT_COUNT;
        break;
    }
}

#ifdef __cplusplus
}
#endif

#endif /*__LINEAR_CONTAINER_H__*/

/*
 * 文件：linear_container.c
 * 描述：线性容器管理器。适用于大量使用小内存的场景。
 * 作者：Li Rongjin
 * 日期：2024-01-07
 **/
#include "linear_container.h"
#include "mem_man.h"
#include "stdint.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "math.h"

#define CONTAINER0_UNIT_SIZE    16
#define CONTAINER1_UNIT_SIZE    32
#define CONTAINER2_UNIT_SIZE    64

#define CONTAINER0_UNIT_COUNT   30
#define CONTAINER1_UNIT_COUNT   30
#define CONTAINER2_UNIT_COUNT   19

/*
 * 功能：设置元数据区的第pos位的状态为0或1。
 * 返回值：无。
 */
static void LCMMetaSetBit(LCMCtnMeta *meta, unsigned int pos, char val)
{
    unsigned int a = pos / 8, b = pos % 8;

    if (val) {
        meta->base[a] |= 1 << b;
    } else {
        meta->base[a] &= ~(1 << b);
    }
}

/*
 * 功能：获取元数据区的第pos位的状态。
 * 返回值：0或1。
 */
static char LCMetaGetBit(LCMCtnMeta *meta, unsigned int pos)
{
    unsigned int a = pos / 8, b = pos % 8;

    return meta->base[a] & (1 << b);
}

/*
 * 功能：线性容器初始化
 * container：线性容器
 * buf: 线性容器管理的内存基地址
 * bufSize：线性容器管理的内存大小
 * pRemain: 给容器分配完内存后的剩余量。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMContainerInit(LCMLinearContainer *container, uint8_t *buf, unsigned int bufSize, unsigned int *pRemain)
{
    unsigned int unitCount, remain;

    if (!container || !buf || (bufSize < MEM_MAN_ALIGN_SIZE))
        return -EINVAL;

    /*容器基地址MEM_MAN_ALIGN_SIZE对齐，容器尺寸在地址对齐后MEM_MAN_ALIGN_SIZE向下取整。*/
    container->base = buf + (MEM_MAN_ALIGN_SIZE - (unsigned int)buf % MEM_MAN_ALIGN_SIZE);
    bufSize -= (MEM_MAN_ALIGN_SIZE - (unsigned int)buf % MEM_MAN_ALIGN_SIZE);
    unitCount = (bufSize * 8) / (container->unitSize * 8 + 1);
    if (unitCount > container->unitCount) {
        remain = bufSize - container->unitCount * container->unitSize;
    } else {
        remain = 0;
        container->unitCount = unitCount;
    }
    *pRemain = remain;
    container->meta.base = container->base + container->unitCount * container->unitSize;
    container->meta.size = bufSize - container->unitCount * container->unitSize;
    return 0;
}

/*
 * 功能：获取一个容器中空闲内存单元的id
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMContainerGetFreeUnitId(LCMLinearContainer *container, unsigned int *pUintId)
{
    unsigned int c;

    for (c = 0; c < container->unitCount; c++) {
        if (!LCMetaGetBit(&container->meta, c)) {
            *pUintId = c;
            return 0;
        }
    }
    return -ENOMEM;
}

/*
 * 功能：从容器中分配一个内存单元。
 * 返回值：成功时返回地址指针，否则返回NULL。
 */
static void *LCMContainerAlloc(LCMLinearContainer *container)
{
    unsigned int freeUnitId;
    int error = 0;

    error = LCMContainerGetFreeUnitId(container, &freeUnitId);
    if (error != -ENOERR)
        return NULL;
    LCMMetaSetBit(&container->meta, freeUnitId, 1);
    return container->base + freeUnitId * container->unitSize;
}

/*
 * 功能：根据地址获取容器中内存单元的id
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMContainerGetUnitId(LCMLinearContainer *container, void *addr, unsigned int *pUnitId)
{
    unsigned int unitId;

    if ((unsigned int)addr % MEM_MAN_ALIGN_SIZE != 0)
        return -EINVAL;
    unitId = ((uint8_t *)addr - container->base) / container->unitSize;
    if (unitId >= container->unitCount)
        return -EINVAL;
    *pUnitId = unitId;
    return 0;
}

/*
 * 功能：容器释放一个内存单元
 * 返回值：无。
 */
static void LCMContainerFree(LCMLinearContainer *container, void *pointer)
{
    int error = 0;
    unsigned int unitId;

    error = LCMContainerGetUnitId(container, pointer, &unitId);
    if (error != -ENOERR)
        return;
    LCMMetaSetBit(&container->meta, unitId, 0);
}

/*
 * 功能：根据请求分配的内存尺寸选择合适的容器
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMSelectContainerIdBySize(LinearContainerMan *lcm, size_t size, unsigned int *pId)
{
    unsigned int u;

    for (u = 0; u < ARRAY_SIZE(lcm->containers); u++) {
        if (size <= lcm->containers[u].unitSize) {
            *pId = u;
            return 0;
        }
    }
    return -ENOMEM;
}

/*
 * 功能：从管理器申请size大小的内存
 * 返回值：成功时返回地址指针，否则返回NULL。
 */
void *LCMAlloc(LinearContainerMan *lcm, size_t size)
{
    unsigned int ctnId;
    int error;

    error = LCMSelectContainerIdBySize(lcm, size, &ctnId);
    if (error != -ENOERR)
        return NULL;
    return LCMContainerAlloc(&lcm->containers[ctnId]);
}

/*
 * 功能：根据地址选择地址对于内存单元所在的容器。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMSelectContainerIdByAddr(LinearContainerMan *lcm, void *addr, unsigned int *pId)
{
    unsigned int u;
    LCMLinearContainer *container;

    for (u = 0; u < ARRAY_SIZE(lcm->containers); u++) {
        container = &lcm->containers[u];
        if ((uint8_t *)addr >= container->base
                && (uint8_t *)addr < container->base + container->unitCount * container->unitSize) {
            *pId = u;
            return 0;
        }
    }
    return -EINVAL;
}

/*
 * 功能：向管理器释放内存空间。
 * 返回值：无。
 */
void LCMFree(LinearContainerMan *lcm, void *pointer)
{
    unsigned int ctnId = 0;
    int error;

    if (!pointer)
        return;
    error = LCMSelectContainerIdByAddr(lcm, pointer, &ctnId);
    if (error != -ENOERR)
        return;
    LCMContainerFree(&lcm->containers[ctnId], pointer);
}

/*
 * 功能：线性容器管理器初始化
 * lcm: 线性容器管理器
 * buf：管理的内存基地址
 * size：管理的内存尺寸
 * 返回值：成功时返回0，否则返回错误码。
 */
int LCMInit(LinearContainerMan *lcm, uint8_t *buf, unsigned int size, uint32_t *pRemain)
{
    int error = 0;
    unsigned int remain;
    size_t i;

    if (!lcm || !buf || (size < MEM_MAN_ALIGN_SIZE) || !pRemain)
        return -EINVAL;

    remain = size;
    memset(lcm, 0, sizeof (*lcm));
    lcm->containers[0].unitSize = CONTAINER0_UNIT_SIZE;
    lcm->containers[0].unitCount = CONTAINER0_UNIT_COUNT;
    lcm->containers[1].unitSize = CONTAINER1_UNIT_SIZE;
    lcm->containers[1].unitCount = CONTAINER1_UNIT_COUNT;
    lcm->containers[2].unitSize = CONTAINER2_UNIT_SIZE;
    lcm->containers[2].unitCount = CONTAINER2_UNIT_COUNT;

    for (i = 0; i < ARRAY_SIZE(lcm->containers); i++) {
        if (remain > 0) {
            error = LCMContainerInit(&lcm->containers[i], &buf[size-remain], remain, &remain);
            if (error != -ENOERR) {
                *pRemain = remain;
                return 0;
            }
        } else {
            break;
        }
    }
    *pRemain = remain;
    return 0;
}

void LCMExample(void)
{
    LinearContainerMan memManx, *memMan = &memManx;
    uint8_t buf[2048];
    unsigned int remain;

    LCMInit(memMan, buf, sizeof (buf), &remain);

    unsigned int i;
    void *addr[50];

    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    for (i = 0; i < ARRAY_SIZE(addr); i++) {
        addr[i] = LCMAlloc(memMan, 16);
        printf("addr: %p\n", addr[i]);
    }

    LCMFree(memMan, addr[2]);
    void *p;
    p = LCMAlloc(memMan, 16);
    printf("addrx: %p\n", p);

    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    for (i = 0; i < ARRAY_SIZE(addr); i++) {
        addr[i] = LCMAlloc(memMan, 17);
        printf("addr: %p\n", addr[i]);
    }

    LCMFree(memMan, addr[2]);

    p = LCMAlloc(memMan, 17);
    printf("addrx: %p\n", p);

    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    for (i = 0; i < ARRAY_SIZE(addr); i++) {
        addr[i] = LCMAlloc(memMan, 33);
        printf("addr: %p\n", addr[i]);
    }

    LCMFree(memMan, addr[2]);

    p = LCMAlloc(memMan, 33);
    printf("addrx: %p\n", p);
}

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

/*块地址对齐MEM_MAN_ALIGN_SIZE后的偏移量*/
#define CHUNK_ALIGN_OFFSET(chunk_addr)                  ((MEM_MAN_ALIGN_SIZE - ((size_t)chunk_addr % MEM_MAN_ALIGN_SIZE)) % MEM_MAN_ALIGN_SIZE)
/*块地址对齐MEM_MAN_ALIGN_SIZE后的地址*/
#define CHUNK_ALIGN_ADDR(chunk_addr)                    ((uint8_t *)(chunk_addr) + CHUNK_ALIGN_OFFSET(chunk_addr))

#define META_GAP_SIZE           32

/*内存单元状态*/
#define UNIT_STATE_FREE         0   /*空闲*/
#define UNIT_STATE_USED         1   /*已被分配*/

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
static char LCMMetaGetBit(LCMCtnMeta *meta, unsigned int pos)
{
    unsigned int a = pos / 8, b = pos % 8;

    return meta->base[a] & (1 << b);
}

/*
 * 功能：设置容器中内存单元的状态。值为：UNIT_STATE_FREE 或 UNIT_STATE_USED
 * 返回值：
 */
static void LCMContainerSetUnitState(LCMLinearContainer *container, unsigned int pos, char val)
{
    LCMMetaSetBit(&container->metas[0], pos, val);
    LCMMetaSetBit(&container->metas[1], pos, val);
}

/*
 * 功能：获取容器中内存单元的状态，两个元数据区的对应位都位空闲时才认为是空闲。
 * 返回值：
 */
static char LCDContainerGetUnitState(LCMLinearContainer *container, unsigned int pos)
{
    if (LCMMetaGetBit(&container->metas[0], pos) == UNIT_STATE_FREE
            && LCMMetaGetBit(&container->metas[1], pos) == UNIT_STATE_FREE)
        return UNIT_STATE_FREE;
    else
        return UNIT_STATE_USED;
}

/*
 * 功能：线性容器初始化
 * container：线性容器
 * buf: 线性容器管理的内存基地址
 * bufSize：线性容器管理的内存大小
 * pRemain: 给容器分配完内存后的剩余量。
 * 返回值：无。
 */
static void LCMContainerInit(LCMLinearContainer *container, uint8_t *buf, size_t bufSize, size_t *pRemain)
{
    size_t maxUnitCount;    /*buf能够初始化的最大内存单元数量*/
    size_t allocMetaSize;   /*分配给元数据区的尺寸*/
    uint8_t *endAddr;
    size_t temp;

    if (!buf || !pRemain || bufSize < META_GAP_SIZE * 4 + MEM_MAN_ALIGN_SIZE
            || container->unitCount == 0
            || container->unitSize == 0
            || container->unitSize % 8 != 0) {
        goto err0;
    }

    temp = bufSize;
    temp -= META_GAP_SIZE * 4;
    temp -= MEM_MAN_ALIGN_SIZE;
    maxUnitCount = temp * 8 / (container->unitSize * 8 + 2);
    if (maxUnitCount == 0)
        goto err0;

    if (maxUnitCount < container->unitCount) {
        container->unitCount = maxUnitCount;
    }
    allocMetaSize = (container->unitCount + 7) / 8;

    container->metas[0].base = buf + META_GAP_SIZE;
    container->base = container->metas[0].base + allocMetaSize + META_GAP_SIZE;
    /*容器基地址MEM_MAN_ALIGN_SIZE对齐。*/
    container->base = CHUNK_ALIGN_ADDR(container->base);
    container->metas[1].base = container->base + container->unitCount * container->unitSize + META_GAP_SIZE;
    endAddr = container->metas[1].base + allocMetaSize + META_GAP_SIZE;
    *pRemain = bufSize - (endAddr - buf);

    container->metas[0].size = allocMetaSize;
    container->metas[1].size = allocMetaSize;
    memset(container->metas[0].base, 0, allocMetaSize);
    memset(container->metas[1].base, 0, allocMetaSize);
    return;

err0:
    container->unitCount = 0;
    container->metas[0].size = 0;
    container->metas[1].size = 0;
    return;
}

/*
 * 功能：获取一个容器中空闲内存单元的id
 * 返回值：成功时返回0，否则返回错误码。
 */
static int LCMContainerGetFreeUnitId(LCMLinearContainer *container, unsigned int *pUintId)
{
    unsigned int c;

    for (c = 0; c < container->unitCount; c++) {
        if (UNIT_STATE_FREE == LCDContainerGetUnitState(container, c)) {
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
    LCMContainerSetUnitState(container, freeUnitId, UNIT_STATE_USED);
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
    LCMContainerSetUnitState(container, unitId, UNIT_STATE_FREE);
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
int LCMInit(LinearContainerMan *lcm, uint8_t *buf, size_t size, size_t *pRemain)
{
    size_t remain;
    size_t i;

    if (!lcm)
        return -EINVAL;
    for (i = 0; i < ARRAY_SIZE(lcm->containers); i++) {
        LCMContainerInitUnit(&lcm->containers[i], i);
    }
    if (!buf) {
        for (i = 0; i < ARRAY_SIZE(lcm->containers); i++) {
            LCMContainerInit(&lcm->containers[i], NULL, 0, NULL);
        }
        return -EINVAL;
    }

    remain = size;
    for (i = 0; i < ARRAY_SIZE(lcm->containers); i++) {
        LCMContainerInit(&lcm->containers[i], &buf[size-remain], remain, &remain);
    }
    if (pRemain)
        *pRemain = remain;
    return 0;
}

static void LCDMetaPrint(LCMCtnMeta *meta, unsigned int id)
{
    printf("............\n");
    printf("meta%d:\n", id);
    printf("meta base: %p(H)\n", meta->base);
    printf("meta size: %u\n", meta->size);
    printf("meta hex image:\n");
    for (uint32_t k = 0; k < meta->size; k++) {
        printf("%02x ", meta->base[k]);
        if (!((k+1) % 16))
            printf("\n");
    }
    printf("\n");
}

static void LCMContainerPrint(LCMLinearContainer *container)
{
    uint32_t freeUnitCount = 0;

    LCDMetaPrint(&container->metas[0], 0);
    LCDMetaPrint(&container->metas[1], 1);
    for (uint32_t t = 0; t < container->unitCount; t++) {
        if (!LCMMetaGetBit(&container->metas[0], t))
            freeUnitCount++;
    }
    printf("............\n");
    printf("container base: %p(H)\n", container->base);
    printf("unit size: %u\n", container->unitSize);
    printf("unit count: %u\n", container->unitCount);
    printf("total space size: %u\n", container->unitCount * container->unitSize);
    printf("free space size: %u\n", freeUnitCount * container->unitSize);
    printf("used space size: %u\n", (container->unitCount - freeUnitCount) * container->unitSize);
}

void LCMPrint(LinearContainerMan *lcm)
{
    uint32_t e;

    for (e = 0; e < ARRAY_SIZE(lcm->containers); e++) {
        printf("------------container%d-------------------------\n", e);
        LCMContainerPrint(&lcm->containers[e]);
    }
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

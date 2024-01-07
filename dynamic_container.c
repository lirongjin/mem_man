/*
 * 文件：dynamic_container.c
 * 描述：实现用于内存管理的动态容器。
 * 作者：Li Rongjin
 * 日期：2024-01-07
 **/

#include "dynamic_container.h"
#include "mem_man.h"
#include "stdint.h"
#include "math.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"

/*边界标记长度*/
#define BOUNDARY_MARKER_SIZE                            8
/*块的最小长度*/
#define CHUNK_MIN_SIZE                                  (BOUNDARY_MARKER_SIZE * 2 + 8)

/*对块尺寸进行向下/上取整，是MEM_MAN_ALIGN_SIZE的整数倍。*/
#define CHUNK_SIZE_ROUND_DOWN(chunk_size)               ((chunk_size) / MEM_MAN_ALIGN_SIZE * MEM_MAN_ALIGN_SIZE)
#define CHUNK_SIZE_ROUND_UP(chunk_size)                 (((chunk_size + (MEM_MAN_ALIGN_SIZE - 1)) / MEM_MAN_ALIGN_SIZE) * MEM_MAN_ALIGN_SIZE)

/*块地址对齐MEM_MAN_ALIGN_SIZE后的偏移量*/
#define CHUNK_ALIGN_OFFSET(chunk_addr)                  ((MEM_MAN_ALIGN_SIZE - ((size_t)chunk_addr % MEM_MAN_ALIGN_SIZE)) % MEM_MAN_ALIGN_SIZE)
/*块地址对齐MEM_MAN_ALIGN_SIZE后的地址*/
#define CHUNK_ALIGN_ADDR(chunk_addr)                    ((uint8_t *)(chunk_addr) + CHUNK_ALIGN_OFFSET(chunk_addr))

/*块大小和窗口大小之间的互相转换*/
#define CHUNK_SIZE_TO_HOLE_SIZE(chunk_size)             ((chunk_size) - BOUNDARY_MARKER_SIZE * 2)
#define HOLE_SIZE_TO_CHUNK_SIZE(hole_size)              ((hole_size) + BOUNDARY_MARKER_SIZE * 2)

/*块的窗口大小*/
#define CHUNK_HOLE_SIZE(chunk_base)                     (CHUNK_SIZE_TO_HOLE_SIZE(BASE_TO_LMARKER(chunk_base)->chunkSize))

/*块基址和左右边界标记之间的转换*/
#define BASE_TO_LMARKER(chunk_base)                     ((DCMBoundaryMarker *)(chunk_base))
#define BASE_TO_RMARKER(chunk_base)                     ((DCMBoundaryMarker *)((uint8_t *)(chunk_base) + BASE_TO_LMARKER(chunk_base)->chunkSize - BOUNDARY_MARKER_SIZE))
#define LMARKER_TO_BASE(l_marker)                       ((uint8_t *)(l_marker))
#define RMARKER_TO_BASE(r_marker)                       ((uint8_t *)(r_marker) + BOUNDARY_MARKER_SIZE - (r_marker)->chunkSize)

/*块基址和左右指针之间的转换*/
#define BASE_TO_LPOINTER(chunk_base)                    ((void *)((uint8_t *)(chunk_base) + BOUNDARY_MARKER_SIZE))
#define BASE_TO_RPOINTER(chunk_base)                    ((void *)((uint8_t *)(chunk_base) + BASE_TO_LMARKER(chunk_base)->chunkSize - BOUNDARY_MARKER_SIZE - 8))
#define LPOINTER_TO_BASE(l_pointer)                     ((uint8_t *)(l_pointer) - BOUNDARY_MARKER_SIZE)
#define RPOINTER_TO_BASE(r_pointer)                     (RMARKER_TO_BASE((DCMBoundaryMarker *)((uint8_t *)(r_pointer) + 8)))

/*读写块指针*/
#define CHUNK_POINTER_DEREF_R(pointer)                  (*(void **)(pointer))
#define CHUNK_POINTER_DEREF_W(pointer, val)             (*(void **)(pointer) = (val))

/*设置块的前向/后向节点*/
#define CHUNK_SET_PREV_NODE(chunk_base, next)           (CHUNK_POINTER_DEREF_W(BASE_TO_LPOINTER(chunk_base), (next)))
#define CHUNK_SET_NEXT_NODE(chunk_base, next)           (CHUNK_POINTER_DEREF_W(BASE_TO_RPOINTER(chunk_base), (next)))

/*读取块的前/后向节点*/
#define CHUNK_GET_PREV_NODE(chunk_base)                 (CHUNK_POINTER_DEREF_R(BASE_TO_LPOINTER(chunk_base)))
#define CHUNK_GET_NEXT_NODE(chunk_base)                 (CHUNK_POINTER_DEREF_R(BASE_TO_RPOINTER(chunk_base)))

/*获取块/容器节点地址*/
#define CHUNK_NODE(chunk_base)                          (BASE_TO_LPOINTER(chunk_base))
#define CONTAINER_NODE(container)                       ((void)((DCMContainer *)NULL == container), (void *)container)

/*左边相邻块的右边界标记*/
#define CHUNK_LNB_RMARKER(chunk_base)                   ((DCMBoundaryMarker *)((uint8_t *)(chunk_base) - BOUNDARY_MARKER_SIZE))
/*右边相邻块的左边界标记*/
#define CHUNK_RNB_LMARKER(chunk_base)                   ((DCMBoundaryMarker *)((uint8_t *)(chunk_base) + BASE_TO_LMARKER(chunk_base)->chunkSize))

#define Pr(...)     printf(__VA_ARGS__), fflush(stdout)

/*边界标记，用于管理一个块。*/
typedef struct {
    uint32_t used;          /*块是否被使用，0：未使用，1：已使用*/
    uint32_t chunkSize;     /*块大小。*/
} DCMBoundaryMarker;

/*
 * 功能：判断容器是否为空
 * 返回值：如果容器为空返回0，否则返回0。
 */
static inline char DCMContainerIsEmpty(DCMContainer *container)
{
    return container->next == CONTAINER_NODE(container)
            && container->prev == CONTAINER_NODE(container);
}

/*
 * 功能：判断地址是否有效
 * 返回值：地址有效返回1，否则返回0。
 */
static inline char DCMAddressIsValid(DynamicCtnMan *dcm, void *address)
{
    uint8_t *addr = (uint8_t *)address;

    return (addr >= dcm->memBase
            && addr < dcm->memBase + dcm->memSize);
}

/*
 * 功能：判断块是否有效
 * 返回值：块有效返回1，否则返回0。
 */
static inline char DCMChunkIsValid(DynamicCtnMan *dcm, uint8_t *chunkBase)
{
    DCMBoundaryMarker *leftMarker, *rightMarker;

    if (DCMAddressIsValid(dcm, chunkBase)) {
        leftMarker = BASE_TO_LMARKER(chunkBase);
        rightMarker = BASE_TO_RMARKER(chunkBase);
        if (chunkBase + leftMarker->chunkSize <= dcm->memBase + dcm->memSize
                && leftMarker->chunkSize == rightMarker->chunkSize
                && leftMarker->used == rightMarker->used) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

/*
 * 功能：取2的对数
 * 返回值：
 */
static unsigned int DCMLog2(unsigned int len)
{
    unsigned int i;

    for (i = 0; ; i++) {
        len /= 2;
        if (len == 0)
            return i;
    }
}

/*
 * 功能：根据块尺寸选择合适的容器
 * 返回值：容器指针
 */
static DCMContainer * DCMSelectChunkContainer(DynamicCtnMan *dcm, uint32_t chunkSize)
{
    unsigned int pos;

    /*根据块的窗口尺寸选择容器，对窗口尺寸取2为底的对数，此对数值作为管理器中容器的索引。*/
    pos = DCMLog2(CHUNK_SIZE_TO_HOLE_SIZE(chunkSize));
    return &dcm->containers[pos];
}

/*
 * 功能：向容器中添加块
 * 返回值：无
 */
static void DCMContainerAddChunk(DCMContainer *container, uint8_t *chunkBase)
{
    /*把块添加到容器中的首部。*/
    if (DCMContainerIsEmpty(container)) {
        container->next = CHUNK_NODE(chunkBase);
        container->prev = CHUNK_NODE(chunkBase);
        CHUNK_SET_PREV_NODE(chunkBase, CONTAINER_NODE(container));
        CHUNK_SET_NEXT_NODE(chunkBase, CONTAINER_NODE(container));
    } else {
        uint8_t *firstChunkBase;

        firstChunkBase = LPOINTER_TO_BASE(container->next);
        container->next = CHUNK_NODE(chunkBase);
        CHUNK_SET_PREV_NODE(firstChunkBase, CHUNK_NODE(chunkBase));
        CHUNK_SET_PREV_NODE(chunkBase, CONTAINER_NODE(container));
        CHUNK_SET_NEXT_NODE(chunkBase, CHUNK_NODE(firstChunkBase));
    }
    container->chunkCnt++;
}

/*
 * 功能：向管理器中添加块
 * 返回值：无
 */
static void DCMAddChunk(DynamicCtnMan *dcm, uint8_t *chunkBase, uint32_t chunkSize)
{
    DCMContainer *container;
    DCMBoundaryMarker *leftMarker, *rightMarker;

    /*推算出块的左右边界标记，并写入块的信息。*/
    leftMarker = BASE_TO_LMARKER(chunkBase);
    leftMarker->used = 0;
    leftMarker->chunkSize = chunkSize;
    rightMarker = BASE_TO_RMARKER(chunkBase);
    rightMarker->used = 0;
    rightMarker->chunkSize = chunkSize;
    /*根据块的窗口尺寸选择合适的容器并将其添加到容器首部。*/
    container = DCMSelectChunkContainer(dcm, chunkSize);
    DCMContainerAddChunk(container, chunkBase);
}

/*
 * 功能：从容器中删除块
 * 返回值：无
 */
static void DCMContainerDelChunk(DCMContainer *container, uint8_t *chunkBase)
{
    void *prevNode, *nextNode;

    /*因为容器是以双向链表的方式组织块的，所以把待删除块的前一个块的后向链接指针
        设置为待删除块的后一个块的节点指针，把待删除块的后一个块的前向链接指针设置为
        待删除块的前一个块的节点指针。*/
    prevNode = CHUNK_GET_PREV_NODE(chunkBase);
    nextNode = CHUNK_GET_NEXT_NODE(chunkBase);
    if (prevNode == CONTAINER_NODE(container)) {
        if (nextNode == CONTAINER_NODE(container)) {
            container->prev = container->next = CONTAINER_NODE(container);
        } else {
            uint8_t *nextChunkBase;

            nextChunkBase = LPOINTER_TO_BASE(nextNode);
            CHUNK_SET_PREV_NODE(nextChunkBase, CONTAINER_NODE(container));
            container->next = CHUNK_NODE(nextChunkBase);
        }
    } else {
        if (nextNode == CONTAINER_NODE(container)) {
            uint8_t *prevChunkBase;

            prevChunkBase = LPOINTER_TO_BASE(prevNode);
            CHUNK_SET_NEXT_NODE(prevChunkBase, CONTAINER_NODE(container));
            container->prev = CHUNK_NODE(prevChunkBase);
        } else {
            uint8_t *prevChunkBase, *nextChunkBase;

            prevChunkBase = LPOINTER_TO_BASE(prevNode);
            nextChunkBase = LPOINTER_TO_BASE(nextNode);
            CHUNK_SET_NEXT_NODE(prevChunkBase, CHUNK_NODE(nextChunkBase));
            CHUNK_SET_PREV_NODE(nextChunkBase, CHUNK_NODE(prevChunkBase));
        }
    }
    container->chunkCnt--;
}

/*
 * 功能：分配容器中的块
 * 返回值：无
 */
static void DCMContainerChunkAlloc(DynamicCtnMan *dcm, DCMContainer *container,
                             uint8_t *chunkBase, size_t allocSize)
{
    DCMBoundaryMarker *leftMarker, *rightMarker;
    unsigned int remain;

    /*如果块尺寸减去被分配的尺寸后大于等于CHUNK_MIN_SIZE，则把剩余尺寸添加到
        管理器中，否则把整个块分配出去。*/
    leftMarker = BASE_TO_LMARKER(chunkBase);
    remain = leftMarker->chunkSize - allocSize - BOUNDARY_MARKER_SIZE * 2;
    DCMContainerDelChunk(container, chunkBase);
    if (remain >= CHUNK_MIN_SIZE) {
        uint8_t *newChunkBase;

        leftMarker->chunkSize = allocSize + BOUNDARY_MARKER_SIZE * 2;
        rightMarker = BASE_TO_RMARKER(chunkBase);
        rightMarker->chunkSize = leftMarker->chunkSize;
        newChunkBase = chunkBase + leftMarker->chunkSize;
        DCMAddChunk(dcm, newChunkBase, remain);
    } else {
        rightMarker = BASE_TO_RMARKER(chunkBase);
    }
    leftMarker->used = 1;
    rightMarker->used = 1;
}

/*
 * 功能：遍历容器中的块找到合适的块进行内存分配。
 * 返回值：成功时返回可用的地址指针，否则返回NULL。
 */
static void *DCMContainerAlloc(DynamicCtnMan *dcm, DCMContainer *container, size_t size)
{
    void *chunkNodeIter;
    uint8_t *chunkBase;

    chunkNodeIter = container->next;
    for ( ;chunkNodeIter != CONTAINER_NODE(container); ) {
        chunkBase = LPOINTER_TO_BASE(chunkNodeIter);
        if (!DCMChunkIsValid(dcm, chunkBase))
            return NULL;
        if (CHUNK_HOLE_SIZE(chunkBase) >= size) {
            DCMContainerChunkAlloc(dcm, container, chunkBase, size);
            return chunkNodeIter;
        }
        chunkNodeIter = CHUNK_GET_NEXT_NODE(chunkBase);
    }
    return NULL;
}

/*
 * 功能：从管理器分配size大小的内存。
 * 返回值：成功时返回可用的地址指针，否则返回NULL。
 */
void *DCMAlloc(DynamicCtnMan *dcm, size_t size)
{
    unsigned int pos;
    unsigned int i;
    void *ret;

    if (size == 0)
        size = 1;
    size = CHUNK_SIZE_ROUND_UP(size);
    pos = DCMLog2(size);
    for (i = pos; i < ARRAY_SIZE(dcm->containers); i++) {
        ret = DCMContainerAlloc(dcm, &dcm->containers[i], size);
        if (ret)
            return ret;
    }
    return NULL;
}

/*
 * 功能：向管理器释放pointer指向的内存空间。
 * 返回值：无
 */
void DCMFree(DynamicCtnMan *dcm, void *pointer)
{
    uint8_t *chunkBase;
    DCMBoundaryMarker *leftMarker;
    DCMBoundaryMarker *lNbRMarker, *rNbLMarker;
    char lNbUsed = 1, rNbUsed = 1;
    unsigned int freeChunkSize;
    uint8_t *freeChunkBase;

    if (!pointer)
        return;

    /*根据待释放指针，获取被释放块的基地址和相邻块的使用信息。
        如果相邻块是空闲的，则进行合并。*/
    chunkBase = LPOINTER_TO_BASE(pointer);
    if (!DCMChunkIsValid(dcm, chunkBase))
        return;
    leftMarker = BASE_TO_LMARKER(chunkBase);
    lNbRMarker = CHUNK_LNB_RMARKER(chunkBase);
    rNbLMarker = CHUNK_RNB_LMARKER(chunkBase);
    if (DCMAddressIsValid(dcm, lNbRMarker) &&
            DCMChunkIsValid(dcm, RMARKER_TO_BASE(lNbRMarker))
            && lNbRMarker->used == 0) {
        lNbUsed = 0;
    }
    if (DCMAddressIsValid(dcm, rNbLMarker) &&
            DCMChunkIsValid(dcm, LMARKER_TO_BASE(rNbLMarker))
            && rNbLMarker->used == 0) {
        rNbUsed = 0;
    }

    /*从管理器中删除相邻空闲块，并跟被释放块合并到一起添加到管理器中。*/
    if (lNbUsed == 0 && rNbUsed == 0) {
        freeChunkSize = lNbRMarker->chunkSize + leftMarker->chunkSize + rNbLMarker->chunkSize;
        freeChunkBase = RMARKER_TO_BASE(lNbRMarker);
        DCMContainerDelChunk(DCMSelectChunkContainer(dcm, lNbRMarker->chunkSize), RMARKER_TO_BASE(lNbRMarker));
        DCMContainerDelChunk(DCMSelectChunkContainer(dcm, rNbLMarker->chunkSize), LMARKER_TO_BASE(rNbLMarker));
    } else if (lNbUsed == 0) {
        freeChunkSize = lNbRMarker->chunkSize + leftMarker->chunkSize;
        freeChunkBase = RMARKER_TO_BASE(lNbRMarker);
        DCMContainerDelChunk(DCMSelectChunkContainer(dcm, lNbRMarker->chunkSize), RMARKER_TO_BASE(lNbRMarker));
    } else if (rNbUsed == 0) {
        freeChunkSize = leftMarker->chunkSize + rNbLMarker->chunkSize;
        freeChunkBase = chunkBase;
        DCMContainerDelChunk(DCMSelectChunkContainer(dcm, rNbLMarker->chunkSize), LMARKER_TO_BASE(rNbLMarker));
    } else {
        freeChunkBase = chunkBase;
        freeChunkSize = leftMarker->chunkSize;
    }
    DCMAddChunk(dcm, freeChunkBase, freeChunkSize);
}

/*
 * 功能：初始化一个动态容器管理器
 * dcm: 动态容器管理器
 * buffer: 动态容器管理器的内存
 * bufLen: 内存大小
 * 返回值：成功时返回0，否则返回错误码。
 */
int DCMInit(DynamicCtnMan *dcm, uint8_t *buffer, uint32_t bufLen)
{
    unsigned int i;

    if (!buffer || bufLen < MEM_MAN_ALIGN_SIZE)
        return -EINVAL;

    /*基地址8字节对齐，可用内存尺寸对齐后向下取整MEM_MAN_ALIGN_SIZE大小。*/
    dcm->memBase = CHUNK_ALIGN_ADDR(buffer);
    bufLen -= CHUNK_ALIGN_OFFSET(buffer);
    bufLen = CHUNK_SIZE_ROUND_DOWN(bufLen);
    if (bufLen < CHUNK_MIN_SIZE)
        return -EINVAL;
    dcm->memSize = bufLen;

    /*初始化管理器中的容器。*/
    for (i = 0; i < ARRAY_SIZE(dcm->containers); i++) {
        dcm->containers[i].prev = &dcm->containers[i];
        dcm->containers[i].next = &dcm->containers[i];
        dcm->containers[i].chunkCnt = 0;
    }
    /*把被管理的内存添加到管理器中。*/
    DCMAddChunk(dcm, dcm->memBase, dcm->memSize);
    return 0;
}

/*
 * 功能：打印容器信息：容器中的块数量，块的尺寸
 * 返回值：无
 */
static void DCMContainerPrint(DynamicCtnMan *dcm, DCMContainer *container, unsigned int *pFreeSize)
{
    void *chunkNodeIter;
    uint8_t *chunkBase;
    unsigned int i = 0;

    printf("chunk count: %u\n", container->chunkCnt);
    chunkNodeIter = container->next;
    for ( ;chunkNodeIter != CONTAINER_NODE(container); ) {
        chunkBase = LPOINTER_TO_BASE(chunkNodeIter);
        if (!DCMChunkIsValid(dcm, chunkBase))
            return;
        printf("chunck%u size: %u byte\n", i++, BASE_TO_LMARKER(chunkBase)->chunkSize);
        *pFreeSize += BASE_TO_LMARKER(chunkBase)->chunkSize;
        chunkNodeIter = CHUNK_GET_NEXT_NODE(chunkBase);
    }
    return;
}

/*
 * 功能：打印动态容器管理器的信息：
 *      容器的使用信息，总共内存尺寸和剩余内存尺寸信息。
 * 返回值：无
 */
void DCMPrint(DynamicCtnMan *dcm)
{
    unsigned int i;
    unsigned int freeSize = 0;

    printf("----------------------DCM----------------------\n");
    for (i = 0; i < ARRAY_SIZE(dcm->containers); i++) {
        if (DCMContainerIsEmpty(&dcm->containers[i]))
            continue;
        printf("container %u:\n", i);
        DCMContainerPrint(dcm, &dcm->containers[i], &freeSize);
        printf("-------------------------------------------\n");
    }
    printf("total memory sapce %u byte\n", dcm->memSize);
    printf("free memory space %u byte\n", freeSize);
    printf("----------------------END----------------------\n");
}

void DCMExample(void)
{
    DynamicCtnMan dcm;
    uint8_t buf[1024];
    uint8_t *p;

    DCMInit(&dcm, buf, sizeof (buf));

    p = DCMAlloc(&dcm, 2);
    printf("p: %p\n", p);
    DCMFree(&dcm, p);

    DCMPrint(&dcm);

}


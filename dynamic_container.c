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
#include "cpl_debug.h"

//#define DCM_DEBUG
//#define DCM_CHECKSUM

#ifdef DCM_DEBUG
#define PrDbg(...)          Pr("", __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

/*边界标记长度*/
#define BOUNDARY_MARKER_SIZE                            8
/*块节点指针长度*/
#define CHUNK_POINT_SIZE                                8
/*块的最小长度*/
#define CHUNK_MIN_SIZE                                  (BOUNDARY_MARKER_SIZE * 2 + CHUNK_POINT_SIZE * 2)

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
#define BASE_TO_RPOINTER(chunk_base)                    ((void *)((uint8_t *)(chunk_base) + BASE_TO_LMARKER(chunk_base)->chunkSize - BOUNDARY_MARKER_SIZE - CHUNK_POINT_SIZE))
#define LPOINTER_TO_BASE(l_pointer)                     ((uint8_t *)(l_pointer) - BOUNDARY_MARKER_SIZE)
#define RPOINTER_TO_BASE(r_pointer)                     (RMARKER_TO_BASE((DCMBoundaryMarker *)((uint8_t *)(r_pointer) + CHUNK_POINT_SIZE)))

/*块校验数据区地址和长度*/
#define CHUNK_CS_DATA_ADDR(chunk_base)                  ((uint8_t *)BASE_TO_LPOINTER(chunk_base) + CHUNK_POINT_SIZE)
#define CHUNK_CS_DATA_LEN(chunk_base)                   (BASE_TO_LMARKER(chunk_base)->chunkSize - BOUNDARY_MARKER_SIZE * 2 - CHUNK_POINT_SIZE * 2)

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

/*块节点和基地址之间的转换*/
#define NODE_TO_CHUNK(node)                             LPOINTER_TO_BASE(node)

/*左边相邻块的右边界标记*/
#define CHUNK_LNB_RMARKER(chunk_base)                   ((DCMBoundaryMarker *)((uint8_t *)(chunk_base) - BOUNDARY_MARKER_SIZE))
/*右边相邻块的左边界标记*/
#define CHUNK_RNB_LMARKER(chunk_base)                   ((DCMBoundaryMarker *)((uint8_t *)(chunk_base) + BASE_TO_LMARKER(chunk_base)->chunkSize))

#define CHUNK_CS_DEFAULT_VAL                               (0x5AA5)

/*边界标记，用于管理一个块。*/
typedef struct {
    uint32_t used: 1;           /*块是否被使用，0：未使用，1：已使用*/
    uint32_t checksum: 16;      /*校验信息*/
    uint32_t chunkSize;         /*块大小。*/
} DCMBoundaryMarker;

#ifdef DCM_CHECKSUM
/*
 * 功能：生成buffer区的校验和
 * 返回值：
 **/
static uint16_t DCMGenChecksum(uint8_t *buffer, size_t size)
{
    uint16_t checksum = 0;
    size_t r;

    for (r = 0; r < size; r++) {
        checksum += buffer[r];
    }
    return checksum;
}
#endif

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

static inline char DCMChunkIsFree(DynamicCtnMan *dcm, uint8_t *chunkBase)
{
    if (DCMChunkIsValid(dcm, chunkBase)) {
        DCMBoundaryMarker *leftMarker;
        DCMBoundaryMarker *rightMarker;
        uint16_t checksum;

        leftMarker = BASE_TO_LMARKER(chunkBase);
        rightMarker = BASE_TO_RMARKER(chunkBase);
#ifdef DCM_CHECKSUM

        checksum = DCMGenChecksum(CHUNK_CS_DATA_ADDR(chunkBase), CHUNK_CS_DATA_LEN(chunkBase));
        if (leftMarker->checksum == rightMarker->checksum
                && leftMarker->checksum == checksum) {
            return leftMarker->used == 0;
        } else {
            if (leftMarker->used == 0) {
                printf("chunk [%p(H)] is invalid\n", chunkBase);
            }
            return 0;
        }
#else
        (void)checksum;
        if (leftMarker->checksum == rightMarker->checksum
                && leftMarker->checksum == CHUNK_CS_DEFAULT_VAL) {
            return leftMarker->used == 0;
        } else {
            return 0;
        }
#endif
    } else {
        return 0;
    }
}

/*
 * 功能：取value以2为底的对数
 * 返回值：value以2为底的对数
 */
static size_t DCMLog2(size_t value)
{
    size_t i;

    for (i = 0; ; i++) {
        value /= 2;
        if (value == 0)
            return i;
    }
}

/*
 * 功能：根据块尺寸选择合适的容器
 * 返回值：容器指针
 */
static DCMContainer * DCMSelectChunkContainer(DynamicCtnMan *dcm, size_t chunkSize)
{
    size_t pos;

    /*根据块的窗口尺寸选择容器，对窗口尺寸取2为底的对数，此对数值作为管理器中容器的索引。*/
    pos = DCMLog2(CHUNK_SIZE_TO_HOLE_SIZE(chunkSize));
    return &dcm->containers[pos];
}

/*
 * 功能：判断两个块是否在同一个容器中。
 * 返回值：如果两个块是否在同一个容器中返回1，否则返回0。
 **/
static char DCMChunksIsInOneContainer(DynamicCtnMan *dcm, uint8_t *lChunk, uint8_t *rChunk)
{
    size_t lChunkSize, rChunkSize;

    if (!DCMAddressIsValid(dcm, lChunk)
            || !DCMChunkIsValid(dcm, lChunk))
        return 0;
    if (!DCMAddressIsValid(dcm, rChunk)
            || !DCMChunkIsValid(dcm, rChunk))
        return 0;

    lChunkSize = BASE_TO_LMARKER(lChunk)->chunkSize;
    rChunkSize = BASE_TO_LMARKER(rChunk)->chunkSize;
    return DCMSelectChunkContainer(dcm, lChunkSize) == DCMSelectChunkContainer(dcm, rChunkSize);
}

/*
 * 功能：根据块的前向链接指针寻找块的下一个有效节点，用于绕过坏块
 * 返回值：
 **/
static void *DCMSearchNextValidNode(DynamicCtnMan *dcm, DCMContainer *container, void *startNode)
{
    void *iterNode, *validNode;

    if (DCMContainerIsEmpty(container))
        return CONTAINER_NODE(container);

    validNode = startNode;
    if (CONTAINER_NODE(container) == startNode) {
        iterNode = container->prev;
    } else {
        iterNode = CHUNK_GET_PREV_NODE(NODE_TO_CHUNK(startNode));
    }
    for (; iterNode != startNode; ) {
        if (CONTAINER_NODE(container) == iterNode) {
            validNode = iterNode;
            iterNode = container->prev;
        }  else {
            /*判断迭代节点和起始节点是否在同一容器中，如果内存内容被非法修改（极端情况），也能退出循环。*/
            if (DCMChunkIsFree(dcm, NODE_TO_CHUNK(iterNode))
                    && DCMChunksIsInOneContainer(dcm, NODE_TO_CHUNK(startNode), NODE_TO_CHUNK(iterNode))) {
                validNode = iterNode;
                iterNode = CHUNK_GET_PREV_NODE(NODE_TO_CHUNK(iterNode));
            } else {
                return validNode;
            }
        }
    }
    return validNode;
}

/*
 * 功能：根据块的后向链接指针寻找块的前一个有效节点，用于绕过坏块
 * 返回值：
 **/
static void *DCMSearchPrevValidNode(DynamicCtnMan *dcm, DCMContainer *container, void *startNode)
{
    void *iterNode, *validNode;

    if (DCMContainerIsEmpty(container))
        return CONTAINER_NODE(container);

    validNode = startNode;
    if (CONTAINER_NODE(container) == startNode) {
        iterNode = container->next;
    } else {
        iterNode = CHUNK_GET_NEXT_NODE(NODE_TO_CHUNK(startNode));
    }
    for (; iterNode != startNode; ) {
        if (CONTAINER_NODE(container) == iterNode) {
            validNode = iterNode;
            iterNode = container->next;
        }  else {
            /*判断迭代节点和起始节点是否在同一容器中，如果内存内容被非法修改（极端情况），也能退出循环。*/
            if (DCMChunkIsFree(dcm, NODE_TO_CHUNK(iterNode))
                    && DCMChunksIsInOneContainer(dcm, NODE_TO_CHUNK(startNode), NODE_TO_CHUNK(iterNode))) {
                validNode = iterNode;
                iterNode = CHUNK_GET_NEXT_NODE(NODE_TO_CHUNK(iterNode));
            } else {
                return validNode;
            }
        }
    }
    return validNode;
}

#define CONTAINER_IS_NO_EMPTY   0
#define CONTAINER_IS_EMPTY      1

/*
 * 功能：把块节点添加到容器的首部
 * 返回值：
 **/
static inline void DCMContainerAddNode(DCMContainer *container, void *chunkNode, uint8_t *nextChunkNode, char isEmpty)
{
    uint8_t *chunkBase;

    chunkBase = NODE_TO_CHUNK(chunkNode);
    /*把块添加到容器中的首部。*/
    if (isEmpty == CONTAINER_IS_EMPTY
            || chunkNode == nextChunkNode) {
        container->next = chunkNode;
        container->prev = chunkNode;
        CHUNK_SET_PREV_NODE(chunkBase, CONTAINER_NODE(container));
        CHUNK_SET_NEXT_NODE(chunkBase, CONTAINER_NODE(container));
    } else {
        container->next = chunkNode;
        CHUNK_SET_PREV_NODE(NODE_TO_CHUNK(nextChunkNode), chunkNode);
        CHUNK_SET_PREV_NODE(chunkBase, CONTAINER_NODE(container));
        CHUNK_SET_NEXT_NODE(chunkBase, nextChunkNode);
    }
}

/*
 * 功能：向容器中添加块
 * 返回值：无
 */
static void DCMContainerAddChunk(DynamicCtnMan *dcm, DCMContainer *container, uint8_t *chunkBase)
{
    /*把块添加到容器中的首部。*/
    if (DCMContainerIsEmpty(container)) {
        DCMContainerAddNode(container, CHUNK_NODE(chunkBase), NULL, CONTAINER_IS_EMPTY);
    } else {
        void *nextNode;

        nextNode = container->next;
        if (!DCMChunkIsFree(dcm, NODE_TO_CHUNK(nextNode))) {    //删除无效节点，使用块前向链接指针寻找有效节点并处理。
            PrDbg("first node [%p(H)] is invalide\n", nextNode);
            nextNode = DCMSearchNextValidNode(dcm, container, CONTAINER_NODE(container));
            if (nextNode == CONTAINER_NODE(container)) {
                container->chunkCnt++;
                DCMContainerAddNode(container, CHUNK_NODE(chunkBase), NULL, CONTAINER_IS_EMPTY);
                return;
            }
        }
        DCMContainerAddNode(container, CHUNK_NODE(chunkBase), nextNode, CONTAINER_IS_NO_EMPTY);
    }
    container->chunkCnt++;
}

/*
 * 功能：向管理器中添加块
 * 返回值：无
 */
static void DCMAddChunk(DynamicCtnMan *dcm, uint8_t *chunkBase, size_t chunkSize)
{
    DCMContainer *container;
    DCMBoundaryMarker *leftMarker, *rightMarker;

    /*推算出块的左右边界标记，并写入块的信息。*/
    leftMarker = BASE_TO_LMARKER(chunkBase);
    leftMarker->used = 0;
    leftMarker->chunkSize = chunkSize;
    rightMarker = BASE_TO_RMARKER(chunkBase);
    *rightMarker = *leftMarker;
    /*根据块的窗口尺寸选择合适的容器并将其添加到容器首部。*/
    container = DCMSelectChunkContainer(dcm, chunkSize);
    DCMContainerAddChunk(dcm, container, chunkBase);
#ifdef DCM_CHECKSUM
    leftMarker->checksum = DCMGenChecksum(CHUNK_CS_DATA_ADDR(chunkBase), CHUNK_CS_DATA_LEN(chunkBase));
    rightMarker->checksum = leftMarker->checksum;
#else
    leftMarker->checksum = CHUNK_CS_DEFAULT_VAL;
    rightMarker->checksum = CHUNK_CS_DEFAULT_VAL;
#endif
}

/*
 * 功能：删除容器中的节点
 * 返回值：
 **/
static void DCMDelNode(DCMContainer *container, void *prevNode, void *nextNode)
{
    /*如果前向节点等于后向节点，则参数无效，容器置空。*/
    if (prevNode == nextNode) {
        container->next = container->prev = CONTAINER_NODE(container);
        return;
    }
    /*因为容器是以双向链表的方式组织块的，所以把待删除块的前一个块的后向链接指针
        设置为待删除块的后一个块的节点指针，把待删除块的后一个块的前向链接指针设置为
        待删除块的前一个块的节点指针。*/
    if (prevNode == CONTAINER_NODE(container)) {
        if (nextNode == CONTAINER_NODE(container)) {
            container->next = container->prev = CONTAINER_NODE(container);
        } else {
            container->next = nextNode;
            CHUNK_SET_PREV_NODE(NODE_TO_CHUNK(nextNode), CONTAINER_NODE(container));
        }
    } else {
        if (nextNode == CONTAINER_NODE(container)) {
            CHUNK_SET_NEXT_NODE(NODE_TO_CHUNK(prevNode), CONTAINER_NODE(container));
            container->prev = prevNode;
        } else {
            CHUNK_SET_NEXT_NODE(NODE_TO_CHUNK(prevNode), nextNode);
            CHUNK_SET_PREV_NODE(NODE_TO_CHUNK(nextNode), prevNode);
        }
    }
}

/*
 * 功能：从容器中删除块
 * 返回值：无
 */
static void DCMContainerDelChunk(DynamicCtnMan *dcm, DCMContainer *container, uint8_t *chunkBase)
{
    void *prevNode, *nextNode;

    prevNode = CHUNK_GET_PREV_NODE(chunkBase);
    nextNode = CHUNK_GET_NEXT_NODE(chunkBase);

    /*使节点代表有效块或者容器*/
    if (prevNode == CONTAINER_NODE(container)) {        /*前向节点是容器*/
        if (nextNode != CONTAINER_NODE(container)) {    /*后向节点是容器*/
            uint8_t *nextChunkBase;

            nextChunkBase = NODE_TO_CHUNK(nextNode);
            if (!DCMChunkIsFree(dcm, nextChunkBase)) {
                PrDbg("first node [%p(H)] is invalide\n", nextChunkBase);
                nextNode = DCMSearchNextValidNode(dcm, container, CHUNK_NODE(chunkBase));
                if (CHUNK_NODE(chunkBase) == nextNode
                        || CONTAINER_NODE(container) == nextNode) {
                    container->prev = container->next = CONTAINER_NODE(container);
                    return;
                }
            }
        }
    } else {                                            /*前向节点是块*/
        if (nextNode == CONTAINER_NODE(container)) {    /*后向节点是容器*/
            uint8_t *prevChunkBase;

            prevChunkBase = NODE_TO_CHUNK(prevNode);
            if (!DCMChunkIsFree(dcm, prevChunkBase)) {
                PrDbg("tail node [%p(H)] is invalide\n", prevChunkBase);
                prevNode = DCMSearchPrevValidNode(dcm, container, CHUNK_NODE(chunkBase));
                if (prevNode == CHUNK_NODE(chunkBase)
                        || CONTAINER_NODE(container) == prevNode) {
                    container->prev = container->next = CONTAINER_NODE(container);
                    return;
                }
            }
        } else {                                        /*后向节点是块*/
            uint8_t *prevChunkBase, *nextChunkBase;

            prevChunkBase = NODE_TO_CHUNK(prevNode);
            if (!DCMChunkIsFree(dcm, prevChunkBase)) {
                PrDbg("prev node [%p(H)] is invalide\n", prevChunkBase);
                prevNode = DCMSearchPrevValidNode(dcm, container, CHUNK_NODE(chunkBase));
                if (prevNode == CHUNK_NODE(chunkBase)) {
                    container->prev = container->next = CONTAINER_NODE(container);
                    return;
                }
            }
            nextChunkBase = NODE_TO_CHUNK(nextNode);
            if (!DCMChunkIsFree(dcm, nextChunkBase)) {
                PrDbg("next node [%p(H)] is invalide\n", nextChunkBase);
                nextNode = DCMSearchNextValidNode(dcm, container, CHUNK_NODE(chunkBase));
                if (CHUNK_NODE(chunkBase) == nextNode) {
                    container->prev = container->next = CONTAINER_NODE(container);
                    return;
                }
            }
        }
    }
    DCMDelNode(container, prevNode, nextNode);
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
    size_t remain;

    /*如果块尺寸减去被分配的尺寸后大于等于CHUNK_MIN_SIZE，则把剩余尺寸添加到
        管理器中，否则把整个块分配出去。*/
    leftMarker = BASE_TO_LMARKER(chunkBase);
    remain = leftMarker->chunkSize - allocSize - BOUNDARY_MARKER_SIZE * 2;
    DCMContainerDelChunk(dcm, container, chunkBase);
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
static void *DCMContainerAlloc(DynamicCtnMan *dcm, DCMContainer *container, size_t allocSize)
{
    void *iterNode, *lastNode;
    uint8_t *chunkBase;
    uint8_t repeat = 0;

    lastNode = CONTAINER_NODE(container);
    iterNode = container->next;
    for ( ;iterNode != CONTAINER_NODE(container); ) {
        chunkBase = NODE_TO_CHUNK(iterNode);
        if (!DCMChunkIsFree(dcm, chunkBase)) {     /*从容器中删除无效块。*/
            if (repeat == 0) {
                repeat = 1;
                PrDbg("next node [%p(H)] is invalide\n", chunkBase);
                iterNode = DCMSearchNextValidNode(dcm, container, lastNode);
                if (iterNode == lastNode
                        || CONTAINER_NODE(container) == iterNode)
                    return NULL;
                chunkBase = NODE_TO_CHUNK(iterNode);
            } else {    /*如果再次找到无效块则直接退出并返回NULL。*/
                return NULL;
            }
        }
        lastNode = iterNode;

        if (CHUNK_HOLE_SIZE(chunkBase) >= allocSize) {
            DCMContainerChunkAlloc(dcm, container, chunkBase, allocSize);
            return iterNode;
        }
        iterNode = CHUNK_GET_NEXT_NODE(chunkBase);
    }
    return NULL;
}

/*
 * 功能：从管理器分配size大小的内存。
 * 返回值：成功时返回可用的地址指针，否则返回NULL。
 */
void *DCMAlloc(DynamicCtnMan *dcm, size_t size)
{
    size_t pos;
    size_t i;
    void *pointer;

    if (size == 0)
        size = 1;
    size = CHUNK_SIZE_ROUND_UP(size);
    pos = DCMLog2(size);
    /*根据请求的内存大小选择合适的容器，如果当前容器返回NULL，则继续从下一级容器分配内测。*/
    for (i = pos; i < ARRAY_SIZE(dcm->containers); i++) {
        pointer = DCMContainerAlloc(dcm, &dcm->containers[i], size);
        if (pointer)
            return pointer;
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
    size_t freeChunkSize;
    uint8_t *freeChunkBase;

    if (!pointer)
        return;
    /*根据待释放指针，获取被释放块的基地址和相邻块的使用信息。
        如果相邻块是空闲的，则进行合并。*/
    chunkBase = LPOINTER_TO_BASE(pointer);
    if (!DCMChunkIsValid(dcm, chunkBase)) {
        PrDbg("node [%p(H)] is invalide\n", chunkBase);
        return;
    }

    leftMarker = BASE_TO_LMARKER(chunkBase);
    lNbRMarker = CHUNK_LNB_RMARKER(chunkBase);
    rNbLMarker = CHUNK_RNB_LMARKER(chunkBase);

    /*判断相邻块是否有效且空闲。*/
    if (DCMAddressIsValid(dcm, lNbRMarker)) {
        if (DCMChunkIsFree(dcm, RMARKER_TO_BASE(lNbRMarker))) {
            lNbUsed = 0;
        }
    }
    if (DCMAddressIsValid(dcm, rNbLMarker)) {
        if (DCMChunkIsFree(dcm, LMARKER_TO_BASE(rNbLMarker))) {
            rNbUsed = 0;
        }
    }

    /*从管理器中删除有效相邻空闲块，并跟被释放块合并到一起添加到管理器中。*/
    if (lNbUsed == 0 && rNbUsed == 0) {
        freeChunkSize = lNbRMarker->chunkSize + leftMarker->chunkSize + rNbLMarker->chunkSize;
        freeChunkBase = RMARKER_TO_BASE(lNbRMarker);
        DCMContainerDelChunk(dcm, DCMSelectChunkContainer(dcm, lNbRMarker->chunkSize), RMARKER_TO_BASE(lNbRMarker));
        DCMContainerDelChunk(dcm, DCMSelectChunkContainer(dcm, rNbLMarker->chunkSize), LMARKER_TO_BASE(rNbLMarker));
    } else if (lNbUsed == 0) {
        freeChunkSize = lNbRMarker->chunkSize + leftMarker->chunkSize;
        freeChunkBase = RMARKER_TO_BASE(lNbRMarker);
        DCMContainerDelChunk(dcm, DCMSelectChunkContainer(dcm, lNbRMarker->chunkSize), RMARKER_TO_BASE(lNbRMarker));
    } else if (rNbUsed == 0) {
        freeChunkSize = leftMarker->chunkSize + rNbLMarker->chunkSize;
        freeChunkBase = chunkBase;
        DCMContainerDelChunk(dcm, DCMSelectChunkContainer(dcm, rNbLMarker->chunkSize), LMARKER_TO_BASE(rNbLMarker));
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
int DCMInit(DynamicCtnMan *dcm, uint8_t *buffer, size_t bufLen)
{
    size_t i;

    if (!dcm)
        return -EINVAL;

    dcm->memBase = NULL;
    dcm->memSize = 0;
    /*初始化管理器中的容器为空。*/
    for (i = 0; i < ARRAY_SIZE(dcm->containers); i++) {
        dcm->containers[i].prev = &dcm->containers[i];
        dcm->containers[i].next = &dcm->containers[i];
        dcm->containers[i].chunkCnt = 0;
    }
    if (!buffer || bufLen < MEM_MAN_ALIGN_SIZE) {
        return -EINVAL;
    }

    /*基地址8字节对齐，可用内存尺寸对齐后向下取整MEM_MAN_ALIGN_SIZE大小。*/
    dcm->memBase = CHUNK_ALIGN_ADDR(buffer);
    bufLen -= CHUNK_ALIGN_OFFSET(buffer);
    bufLen = CHUNK_SIZE_ROUND_DOWN(bufLen);
    if (bufLen < CHUNK_MIN_SIZE)
        return -EINVAL;
    dcm->memSize = bufLen;

    /*把被管理的内存添加到管理器中。*/
    DCMAddChunk(dcm, dcm->memBase, dcm->memSize);
    return 0;
}

/*
 * 功能：打印容器信息：容器中的块数量，块的尺寸
 * 返回值：无
 */
static void DCMContainerPrint(DynamicCtnMan *dcm, DCMContainer *container, size_t *pFreeSize)
{
    void *iterNode, *lastNode;
    uint8_t *chunkBase;
    size_t i = 0;
    size_t chunkCount = 0;

    lastNode = CONTAINER_NODE(container);
    iterNode = container->next;
    for ( ;iterNode != CONTAINER_NODE(container); ) {
        chunkBase = NODE_TO_CHUNK(iterNode);
        if (!DCMChunkIsFree(dcm, chunkBase)) {
            iterNode = DCMSearchNextValidNode(dcm, container, lastNode);
            if (iterNode == CONTAINER_NODE(container)
                    || iterNode == lastNode) {
                break;
            }
        }
        chunkCount++;
        chunkBase = NODE_TO_CHUNK(iterNode);
        lastNode = iterNode;
        printf("chunck%u size: %u byte\n", i++, BASE_TO_LMARKER(chunkBase)->chunkSize);
        *pFreeSize += BASE_TO_LMARKER(chunkBase)->chunkSize;
        iterNode = CHUNK_GET_NEXT_NODE(chunkBase);
    }
    printf("chunk count: %u\n", container->chunkCnt);
    printf("valid chunk count: %u\n", chunkCount);

    return;
}

/*
 * 功能：打印动态容器管理器的信息：
 *      容器的使用信息，总共内存尺寸和剩余内存尺寸信息。
 * 返回值：无
 */
void DCMPrint(DynamicCtnMan *dcm)
{
    size_t i;
    size_t freeSize = 0;

    printf("----------------------DCM----------------------\n");
    for (i = 0; i < ARRAY_SIZE(dcm->containers); i++) {
        if (DCMContainerIsEmpty(&dcm->containers[i]))
            continue;
        printf("container [%p(H)] %u:\n", &dcm->containers[i], i);
        DCMContainerPrint(dcm, &dcm->containers[i], &freeSize);
        printf("-------------------------------------------\n");
    }
    printf("memory sapce base: %p(H)\n", dcm->memBase);
    printf("total memory sapce %u byte\n", dcm->memSize);
    printf("free memory space %u byte\n", freeSize);
    printf("used memory space %u byte\n", dcm->memSize - freeSize);
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


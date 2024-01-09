/*
 * 文件：mem_man.c
 * 描述：内存管理器，动态的管理一片内存。
 *      用户请求分配内存时先从线性容器管理器中
 *      寻找空闲的内存空间，如果没有则从
 *      动态容器管理器中寻找可用内存。
 * 作者：Li Rongjin
 * 日期：2024-01-07
 **/

#include "mem_man.h"
#include "stdio.h"

/*
 * 功能：内存管理器初始化
 * buf: 内存管理器管理的内存基地址
 * size: 内存管理器管理的内存尺寸
 * 返回值：成功时返回0，否则返回错误码。
 */
int MMInit(MemMan *memMan, uint8_t *buf, unsigned int size)
{
    unsigned int remain = 0;
    int error1, error2;

    error1 = LCMInit(&memMan->lcm, buf, size, &remain);
    error2 = DCMInit(&memMan->dcm, &buf[size-remain], remain);
    if (error1 == -ENOERR &&
            error2 == -ENOERR)
        return 0;
    else
        return -EINVAL;
}

/*
 * 功能：申请size大小的一块内存
 * 返回值：成功时返回有效的被分配内存首地址，否则返回NULL。
 */
void *MMAlloc(MemMan *memMan, size_t size)
{
    void *p;

    p = LCMAlloc(&memMan->lcm, size);
    if (!p)
        return DCMAlloc(&memMan->dcm, size);
    return p;
}

/*
 * 功能：释放指针pointer所指的内存空间。
 * 返回值：无。
 */
void MMFree(MemMan *memMan, void *pointer)
{
    uint8_t *addr = pointer;

    if (addr >= memMan->dcm.memBase) {
        DCMFree(&memMan->dcm, pointer);
    } else {
        LCMFree(&memMan->lcm, pointer);
    }
}

void MMExample(void)
{
    MemMan man;
    uint8_t buf[36];
    void *p;

    MMInit(&man, buf, sizeof (buf));
    p = MMAlloc(&man, 19);
    printf("p: %p\n", p);
    //MMFree(&man, p);
    LCMPrint(&man.lcm);
    DCMPrint(&man.dcm);
}

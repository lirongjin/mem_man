#ifndef __LINEAR_CONTAINER_DEFINE_H__
#define __LINEAR_CONTAINER_DEFINE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*pow(2, id+1)定义，id为容器编号*/
#define CTN0_SZ     (2)
#define CTN1_SZ     (2 * CTN0_SZ)
#define CTN2_SZ     (2 * CTN1_SZ)
#define CTN3_SZ     (2 * CTN2_SZ)
#define CTN4_SZ     (2 * CTN3_SZ)
#define CTN5_SZ     (2 * CTN4_SZ)
#define CTN6_SZ     (2 * CTN5_SZ)
#define CTN7_SZ     (2 * CTN6_SZ)

#define CTN8_SZ     (2 * CTN7_SZ)
#define CTN9_SZ     (2 * CTN8_SZ)
#define CTN10_SZ    (2 * CTN9_SZ)
#define CTN11_SZ    (2 * CTN10_SZ)
#define CTN12_SZ    (2 * CTN11_SZ)
#define CTN13_SZ    (2 * CTN12_SZ)
#define CTN14_SZ    (2 * CTN13_SZ)
#define CTN15_SZ    (2 * CTN14_SZ)

#define CTN16_SZ    (2 * CTN15_SZ)
#define CTN17_SZ    (2 * CTN16_SZ)
#define CTN18_SZ    (2 * CTN17_SZ)
#define CTN19_SZ    (2 * CTN18_SZ)
#define CTN20_SZ    (2 * CTN19_SZ)
#define CTN21_SZ    (2 * CTN20_SZ)
#define CTN22_SZ    (2 * CTN21_SZ)
#define CTN23_SZ    (2 * CTN22_SZ)

#define CTN24_SZ    (2 * CTN23_SZ)
#define CTN25_SZ    (2 * CTN24_SZ)
#define CTN26_SZ    (2 * CTN25_SZ)
#define CTN27_SZ    (2 * CTN26_SZ)
#define CTN28_SZ    (2 * CTN27_SZ)
#define CTN29_SZ    (2 * CTN28_SZ)
#define CTN30_SZ    (2ULL * CTN29_SZ)
#define CTN31_SZ    (2ULL * CTN30_SZ)

/*容器数量*/
#define CONTAINER_SIZE          6

/*各容器内存单元尺寸和数量定义。
 * 内存单元尺寸大小需要满足被8整除（因为分配出去的地址要8字节对齐），没有其他要求。
 * 为计算方便，可用容器的内存单元尺寸可定义为pow(2, id+1), id表示容器编号。*/
#define CONTAINER0_UNIT_SIZE    0
#define CONTAINER0_UNIT_COUNT   0
#define CONTAINER1_UNIT_SIZE    0
#define CONTAINER1_UNIT_COUNT   0
#define CONTAINER2_UNIT_SIZE    0
#define CONTAINER2_UNIT_COUNT   0
/*内存单元从16字节开始*/
#define CONTAINER3_UNIT_SIZE    CTN3_SZ     /*16*/
#define CONTAINER3_UNIT_COUNT   400
#define CONTAINER4_UNIT_SIZE    CTN4_SZ     /*32*/
#define CONTAINER4_UNIT_COUNT   200
#define CONTAINER5_UNIT_SIZE    CTN5_SZ     /*64*/
#define CONTAINER5_UNIT_COUNT   50
#define CONTAINER6_UNIT_SIZE    CTN6_SZ     /*128*/
#define CONTAINER6_UNIT_COUNT   0
#define CONTAINER7_UNIT_SIZE    CTN7_SZ     /*256*/
#define CONTAINER7_UNIT_COUNT   0

#define CONTAINER8_UNIT_SIZE    CTN8_SZ
#define CONTAINER8_UNIT_COUNT   0
#define CONTAINER9_UNIT_SIZE    CTN9_SZ
#define CONTAINER9_UNIT_COUNT   0
#define CONTAINER10_UNIT_SIZE   CTN10_SZ
#define CONTAINER10_UNIT_COUNT  0
#define CONTAINER11_UNIT_SIZE   CTN11_SZ
#define CONTAINER11_UNIT_COUNT  0
#define CONTAINER12_UNIT_SIZE   CTN12_SZ
#define CONTAINER12_UNIT_COUNT  0
#define CONTAINER13_UNIT_SIZE   CTN13_SZ
#define CONTAINER13_UNIT_COUNT  0
#define CONTAINER14_UNIT_SIZE   CTN14_SZ
#define CONTAINER14_UNIT_COUNT  0
#define CONTAINER15_UNIT_SIZE   CTN15_SZ
#define CONTAINER15_UNIT_COUNT  0

#define CONTAINER16_UNIT_SIZE   CTN16_SZ
#define CONTAINER16_UNIT_COUNT  0
#define CONTAINER17_UNIT_SIZE   CTN17_SZ
#define CONTAINER17_UNIT_COUNT  0
#define CONTAINER18_UNIT_SIZE   CTN18_SZ
#define CONTAINER18_UNIT_COUNT  0
#define CONTAINER19_UNIT_SIZE   CTN19_SZ
#define CONTAINER19_UNIT_COUNT  0
#define CONTAINER20_UNIT_SIZE   CTN20_SZ
#define CONTAINER20_UNIT_COUNT  0
#define CONTAINER21_UNIT_SIZE   CTN21_SZ
#define CONTAINER21_UNIT_COUNT  0
#define CONTAINER22_UNIT_SIZE   CTN22_SZ
#define CONTAINER22_UNIT_COUNT  0
#define CONTAINER23_UNIT_SIZE   CTN23_SZ
#define CONTAINER23_UNIT_COUNT  0

#define CONTAINER24_UNIT_SIZE   CTN24_SZ
#define CONTAINER24_UNIT_COUNT  0
#define CONTAINER25_UNIT_SIZE   CTN25_SZ
#define CONTAINER25_UNIT_COUNT  0
#define CONTAINER26_UNIT_SIZE   CTN26_SZ
#define CONTAINER26_UNIT_COUNT  0
#define CONTAINER27_UNIT_SIZE   CTN27_SZ
#define CONTAINER27_UNIT_COUNT  0
#define CONTAINER28_UNIT_SIZE   CTN28_SZ
#define CONTAINER28_UNIT_COUNT  0
#define CONTAINER29_UNIT_SIZE   CTN29_SZ
#define CONTAINER29_UNIT_COUNT  0
#define CONTAINER30_UNIT_SIZE   CTN30_SZ
#define CONTAINER30_UNIT_COUNT  0
#define CONTAINER31_UNIT_SIZE   0
#define CONTAINER31_UNIT_COUNT  0

#ifdef __cplusplus
}
#endif

#endif /*__LINEAR_CONTAINER_DEFINE_H__*/

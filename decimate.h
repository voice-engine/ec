#ifndef __DECIMATE_H__
#define __DECIMATE_H__

#include <stdint.h>

/**
 * @brief Instance structure for the floating-point FIR Decimation.
 */
typedef struct
{
    uint8_t  M;            /**< Decimation Factor. */
    uint16_t numTaps;      /**< Length of the filter. */
    uint16_t blockSize;
    uint16_t outBlockSize;
    float  *pCoeffs;      /**< Points to the coefficient array. The array is of length numTaps.*/
    int32_t  *pState;       /**< Points to the state variable array. The array is of length numTaps+maxBlockSize-1. */
} decimate_t;


extern float coeffs[];

extern int decimate_init (
    decimate_t * S,
    uint16_t numTaps,
    uint8_t M,
    float * pCoeffs,
    int32_t * pState,
    uint32_t blockSize);
    
extern void decimate_process (const decimate_t * S,
                                int32_t * pSrc,
                                int32_t * pDst);

#endif // __DECIMATE_H__

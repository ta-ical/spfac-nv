// #include <cstdio>
// #include <cstdlib>
#include <cuda.h>
#include "icalcu.h"

// __global__ void reduce (ptr_int gd)
// {
//     __shared__ int sdata[THREADS];

//     unsigned tid = threadIdx.x;
//     unsigned id = tid + (2 * blockDim.x) * blockIdx.x;        // halve the blocks
//     sdata[tid] = (gd[id] | gd[id + blockDim.x]);              // first level reduction
//     __syncthreads();

//     for (unsigned s = blockDim.x / 2; s > 0; s >>= 1)
//     {
//         if (tid < s)
//         {
//             sdata[tid] |= sdata[tid + s];
//         }
//         __syncthreads();
//     }

//     if (tid == 0) 
//     {
//         gd[blockIdx.x] = sdata[0];
//     }
// }

__global__ void localMerge (ptr_int data, int superBlock)
{
    extern __shared__ int sdata[];

    unsigned id = threadIdx.x;
    unsigned gid = id + blockDim.x * blockIdx.x;
    sdata[id] = data[gid];

    #pragma unroll(64)
    for (int innerBlock = THREADS; innerBlock > 1; innerBlock >>= 1)
    {
        unsigned stride = innerBlock >> 1;  // = half
        unsigned idx = id + id / stride * stride;
        if (idx < THREADS)
        {
            sorter(sdata[idx], sdata[idx + stride], gid & (superBlock >> 1));
        }
        __syncthreads();
    }
    
    data[gid] = sdata[id];
}

__global__ void globalMerge (ptr_int data, int blockSize, int superBlock)
{
    unsigned id = threadIdx.x + blockDim.x * blockIdx.x;
    unsigned stride = blockSize >> 1;  // = half
    unsigned idx = id + id / stride * stride;
    if (idx < NUM_VALS)
    {
        sorter(data[idx], data[idx + stride], id & (superBlock >> 1));
    }
}

__global__ void localSort (ptr_int data)
{
    extern __shared__ int sdata[];
    
    unsigned id = threadIdx.x;
    unsigned gid = id + blockDim.x * blockIdx.x;
    sdata[id] = data[gid];

    #pragma unroll(64)
    for (int block = 2; block <= THREADS; block <<= 1)
    {
        /* Merging */
        for (int innerBlock = block; innerBlock > 1; innerBlock >>= 1)
        {
            unsigned stride = innerBlock >> 1;  // = half
            unsigned idx = id + id / stride * stride;
            if (idx < THREADS)
            {
                printf("%d %d %d\n", gid, idx, gid & (block >> 1));
                sorter(sdata[idx], sdata[idx + stride], gid & (block >> 1));
            }
            __syncthreads();
        }
    }
    __syncthreads();

    data[gid] = sdata[id];
}

__device__ __forceinline__ void sorter (int& a, int& b, int desc)
{
    int _a = a;
    int _b = b;

    if ((!desc && a <= b) || (desc && a >= b)) 
        return;
    
    _a ^= _b ^= _a ^= _b;
    a = _a;
    b = _b;
}

// #include "spfac_vars.h"

// // kernel1
// __global__
// void spfac_kernel_1(__constant__ unsigned char *xlat_case,
//         __device__ int *result,
//         const __device__ unsigned char *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     int tid = blockIdx * blockDim + threadIdx;
//     tid *= UNROLL_SIZE;
//     prefetch((text + tid), UNROLL_SIZE);

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state = 0;
//         for ( i = tid + j; ; i++ ){
//             state_index = xlat_case[text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];
//             if (state == 0){
//                 break;
//             }
//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 break;
//             }
//         }
//     }
// }

// // kernel2: use LDS
// __global__
// void spfac_kernel_2(__constant__ unsigned char *xlat_case,
//         __device__ int *result,
//         const __device__ unsigned char *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     __shared__ unsigned char buf_xc[SPFAC_ALPHABET_SIZE];
//     __shared__ unsigned char buf_text[GROUP_SIZE * (UNROLL_SIZE + 1)];
//     __shared__ int buf_st[SPFAC_ALPHABET_SIZE];

//     int gid = blockIdx;
//     int lid = threadIdx;
//     gid *= GROUP_SIZE * UNROLL_SIZE;

// #pragma unroll
//     for (i = 0; i < SPFAC_ALPHABET_SIZE; i += GROUP_SIZE){
//         buf_xc[i + lid] = xlat_case[i + lid];
//         buf_st[i + lid] = state_table[i +lid];
//     }

// #pragma unroll
//     for (i = 0; i < GROUP_SIZE * (UNROLL_SIZE + 1); i += GROUP_SIZE){
//         buf_text[i + lid] = text[i + gid + lid];
//     }

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state_index = buf_xc[buf_text[lid * UNROLL_SIZE + j]];
//         pre_state = 0;
//         state = buf_st[state_index];
//         for ( i = lid * UNROLL_SIZE + j + 1; ; i++ ){
//             if (state == 0){
//                 break;
//             }
//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 break;
//             }
//             state_index = buf_xc[buf_text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];
//         }
//     }
// }

// // kernel3: vload
// __global__
// void spfac_kernel_3(__constant__ uchar *xlat_case,
//         __device__ int *result,
//         const __device__ uchar *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     __shared__ uchar4 _buf_xc[SPFAC_ALPHABET_SIZE / 4];
//     __shared__ uchar16 _buf_text[GROUP_SIZE / 16 * (UNROLL_SIZE + 1)];
//     __shared__ int4 _buf_st[SPFAC_ALPHABET_SIZE / 4];

//     __shared__ uchar *buf_xc = (__shared__ uchar *)(_buf_xc);
//     __shared__ uchar *buf_text = (__shared__ uchar *)(_buf_text);
//     __shared__ int *buf_st = (__shared__ int *)(_buf_st);

//     int gid = blockIdx;
//     int lid = threadIdx;
//     gid *= GROUP_SIZE * UNROLL_SIZE;
//     int gid2 = gid / 16;

//     _buf_xc[lid] = vload4(lid, xlat_case);
//     _buf_st[lid] = vload4(lid, state_table);

//     _buf_text[lid] = vload16((gid2 + lid), text);
//     _buf_text[GROUP_SIZE + lid] = vload16((gid2+ GROUP_SIZE + lid), text);
//     buf_text[GROUP_SIZE * UNROLL_SIZE + lid] = text[gid + GROUP_SIZE * UNROLL_SIZE + lid];

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state_index = buf_xc[buf_text[lid * UNROLL_SIZE + j]];
//         pre_state = 0;
//         state = buf_st[state_index];
//         for ( i = lid * UNROLL_SIZE + j + 1; ; i++ ){
//             if (state == 0){
//                 break;
//             }
//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 break;
//             }
//             state_index = buf_xc[buf_text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];
//         }
//     }
// }

// // kernel4: reduce bank conflict
// // BUG!
// struct _text_struct{
//     uchar16 text[2];
//     int unused;
// };
// typedef struct _text_struct text_struct;

// __global__
// void spfac_kernel_4(__constant__ uchar *xlat_case,
//         __device__ int *result,
//         const __device__ uchar *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     __shared__ uchar4 _buf_xc[SPFAC_ALPHABET_SIZE / 4];
//     __shared__ text_struct _buf_text[GROUP_SIZE / 32 * (UNROLL_SIZE + 1)];
//     __shared__ int4 _buf_st[SPFAC_ALPHABET_SIZE / 4];

//     __shared__ uchar *buf_xc = (__shared__ uchar *)(_buf_xc);
//     __shared__ uchar *buf_text = (__shared__ uchar *)(_buf_text);
//     __shared__ int *buf_st = (__shared__ int *)(_buf_st);

//     int gid = blockIdx;
//     int lid = threadIdx;
//     gid *= GROUP_SIZE * UNROLL_SIZE;
//     int gid2 = gid / 16;

//     _buf_xc[lid] = vload4(lid, xlat_case);
//     _buf_st[lid] = vload4(lid, state_table);

//     _buf_text[lid >> 1].text[lid & 0x01] = vload16((gid2 + lid), text);
//     _buf_text[GROUP_SIZE / 2 + (lid >> 1)].text[lid & 0x01] = vload16((gid2+ GROUP_SIZE + lid), text);
//     buf_text[GROUP_SIZE * (UNROLL_SIZE + 4) + lid] = text[gid + GROUP_SIZE * UNROLL_SIZE + lid];

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state_index = buf_xc[buf_text[lid * (UNROLL_SIZE + 4) + j]];
//         pre_state = 0;
//         state = buf_st[state_index];
//         for ( i = lid * (UNROLL_SIZE + 4) + j + 1; ; i++ ){//BUG!!
//             if (state == 0){
//                 break;
//             }
//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 break;
//             }
//             state_index = buf_xc[buf_text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];
//         }
//     }
// }

// // kernel:5 PFAC(base on kernel3)
// __global__
// void spfac_kernel_5(__constant__ uchar *xlat_case,
//         __device__ int *result,
//         const __device__ uchar *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     __shared__ uchar4 _buf_xc[SPFAC_ALPHABET_SIZE / 4];
//     __shared__ uchar16 _buf_text[GROUP_SIZE / 16 * (UNROLL_SIZE + 1)];
//     __shared__ int4 _buf_st[SPFAC_ALPHABET_SIZE / 4];

//     __shared__ uchar *buf_xc = (__shared__ uchar *)(_buf_xc);
//     __shared__ uchar *buf_text = (__shared__ uchar *)(_buf_text);
//     __shared__ int *buf_st = (__shared__ int *)(_buf_st);

//     int gid = blockIdx;
//     int lid = threadIdx;
//     gid *= GROUP_SIZE * UNROLL_SIZE;
//     int gid2 = gid / 16;

//     _buf_xc[lid] = vload4(lid, xlat_case);
//     _buf_st[lid] = vload4(lid, state_table);

//     _buf_text[lid] = vload16((gid2 + lid), text);
//     _buf_text[GROUP_SIZE + lid] = vload16((gid2+ GROUP_SIZE + lid), text);
//     buf_text[GROUP_SIZE * UNROLL_SIZE + lid] = text[gid + GROUP_SIZE * UNROLL_SIZE + lid];

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state_index = buf_xc[buf_text[UNROLL_SIZE * j + lid]];
//         pre_state = 0;
//         state = buf_st[state_index];
//         for ( i = UNROLL_SIZE * j + lid + 1; ; i++ ){
//             if (state == 0){
//                 break;
//             }
//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 break;
//             }
//             state_index = buf_xc[buf_text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];
//         }
//     }
// }

// // kernel6
// __global__
// void spfac_kernel_6(__constant__ uchar *xlat_case,
//         __device__ int *result,
//         const __device__ uchar *text,
//         const __device__ int *state_table)
// {
//     int pre_state, state;
// 	int num_match = 0;
// 	int state_index;

//     int i, j;

//     __shared__ uchar buf_xc[SPFAC_ALPHABET_SIZE];
//     __shared__ uchar buf_text[GROUP_SIZE * (UNROLL_SIZE + 1)];
//     __shared__ int buf_st[SPFAC_ALPHABET_SIZE];

//     int gid = blockIdx;
//     int lid = threadIdx;
//     gid *= GROUP_SIZE * UNROLL_SIZE;
//     int gid2 = gid / 16;

//     ((__shared__ uchar4 *)buf_xc)[lid] = vload4(lid, xlat_case);
//     ((__shared__ int4 *)buf_st)[lid] = vload4(lid, state_table);

// #pragma unroll
//     for (i = 0; i < GROUP_SIZE / 16 * UNROLL_SIZE; i += GROUP_SIZE){
//         ((__shared__ uchar16 *)buf_text)[lid] = vload16((gid2 + i + lid), text);
//     }
//     buf_text[GROUP_SIZE * UNROLL_SIZE + lid] = text[gid + GROUP_SIZE * UNROLL_SIZE + lid];

// #pragma unroll
//     for (j = 0; j < UNROLL_SIZE; j++){
//         state_index = buf_xc[buf_text[lid * UNROLL_SIZE + j]];
//         pre_state = 0;
//         state = buf_st[state_index];
//         i = lid * UNROLL_SIZE + j + 1;
//         while (state != 0){
//             state_index = buf_xc[buf_text[i]];
//             pre_state = state;
//             state = state_table[state * SPFAC_ALPHABET_SIZE + state_index];

//             if (state == -1){
//                 num_match = atomic_inc(result);
//                 result[2 * num_match + 1] = pre_state;
//                 result[2 * num_match + 2] = i;
//                 state = 0;
//             }
//             i++;
//         }
//     }
// }


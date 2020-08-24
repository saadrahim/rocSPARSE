/* ************************************************************************
 * Copyright 2018-2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ************************************************************************ */

#pragma once
#ifndef CSR2CSC_DEVICE_H
#define CSR2CSC_DEVICE_H

#include <hip/hip_runtime.h>

template <typename T, unsigned int BLOCKSIZE>
__launch_bounds__(BLOCKSIZE) __global__ void csr2csc_permute_kernel(rocsparse_int        nnz,
                                                                    const rocsparse_int* in1,
                                                                    const T*             in2,
                                                                    const rocsparse_int* map,
                                                                    rocsparse_int*       out1,
                                                                    T*                   out2)
{
    rocsparse_int gid = hipBlockIdx_x * BLOCKSIZE + hipThreadIdx_x;

    if(gid >= nnz)
    {
        return;
    }

    rocsparse_int idx = map[gid];

    out1[gid] = in1[idx];
    out2[gid] = in2[idx];
}

#endif // CSR2CSC_DEVICE_H

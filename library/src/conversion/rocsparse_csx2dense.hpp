/* ************************************************************************
 * Copyright 2020 Advanced Micro Devices, Inc.
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
#include "csx2dense_device.h"

template <rocsparse_direction DIRA, typename T>
rocsparse_status rocsparse_csx2dense_template(rocsparse_handle          handle,
                                              rocsparse_int             m,
                                              rocsparse_int             n,
                                              const rocsparse_mat_descr descr,
                                              const T*                  csx_val,
                                              const rocsparse_int*      csx_row_col_ptr,
                                              const rocsparse_int*      csx_col_row_ind,
                                              T*                        A,
                                              rocsparse_int             ld)
{
    if(0 == m || 0 == n)
    {
        return rocsparse_status_success;
    }

    hipStream_t stream = handle->stream;

    switch(DIRA)
    {
    case rocsparse_direction_row:
    {
        static constexpr rocsparse_int WAVEFRONT_SIZE  = 64;
        static constexpr rocsparse_int NROWS_PER_BLOCK = 16;

        rocsparse_int blocks = (m - 1) / NROWS_PER_BLOCK + 1;
        dim3          k_blocks(blocks), k_threads(WAVEFRONT_SIZE * NROWS_PER_BLOCK);

        hipLaunchKernelGGL((csr2dense_kernel<NROWS_PER_BLOCK, WAVEFRONT_SIZE, T>),
                           k_blocks,
                           k_threads,
                           0,
                           stream,
                           descr->base,
                           m,
                           n,
                           csx_val,
                           csx_row_col_ptr,
                           csx_col_row_ind,
                           A,
                           ld);

        return rocsparse_status_success;
    }

    case rocsparse_direction_column:
    {
        static constexpr rocsparse_int WAVEFRONT_SIZE     = 64;
        static constexpr rocsparse_int NCOLUMNS_PER_BLOCK = 16;

        rocsparse_int blocks = (n - 1) / NCOLUMNS_PER_BLOCK + 1;
        dim3          k_blocks(blocks), k_threads(WAVEFRONT_SIZE * NCOLUMNS_PER_BLOCK);

        hipLaunchKernelGGL((csc2dense_kernel<NCOLUMNS_PER_BLOCK, WAVEFRONT_SIZE, T>),
                           k_blocks,
                           k_threads,
                           0,
                           stream,
                           descr->base,
                           m,
                           n,
                           csx_val,
                           csx_row_col_ptr,
                           csx_col_row_ind,
                           A,
                           ld);

        return rocsparse_status_success;
    }
    }

    return rocsparse_status_invalid_value;
}

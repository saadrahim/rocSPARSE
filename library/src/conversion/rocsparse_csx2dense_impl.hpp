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
#include "definitions.h"
#include "rocsparse.h"
#include "rocsparse_csx2dense.hpp"
#include "utility.h"
#include <hip/hip_runtime.h>
#include <rocprim/rocprim.hpp>

template <rocsparse_direction DIRA, typename T>
rocsparse_status rocsparse_csx2dense_impl(rocsparse_handle          handle,
                                          rocsparse_int             m,
                                          rocsparse_int             n,
                                          const rocsparse_mat_descr descr,
                                          const T*                  csx_val,
                                          const rocsparse_int*      csx_row_col_ptr,
                                          const rocsparse_int*      csx_col_row_ind,
                                          T*                        A,
                                          rocsparse_int             lda)
{
    static constexpr bool is_row_oriented = (rocsparse_direction_row == DIRA);
    //
    // Checks for valid handle
    //
    if(nullptr == handle)
    {
        return rocsparse_status_invalid_handle;
    }

    //
    // Loggings
    //
    log_trace(handle,
              is_row_oriented ? "rocsparse_csr2dense" : "rocsparse_csc2dense",
              m,
              n,
              descr,
              (const void*&)A,
              lda,
              (const void*&)csx_val,
              (const void*&)csx_row_col_ptr,
              (const void*&)csx_col_row_ind);

    log_bench(handle,
              "./rocsparse-bench",
              "-f",
              is_row_oriented ? "csr2dense" : "csc2dense",
              "-m",
              m,
              "-n",
              n,
              "--denseld",
              lda,
              "--indexbaseA",
              descr->base);

    //
    // Check sizes
    //
    if((m < 0) || (n < 0) || (lda < m))
    {
        return rocsparse_status_invalid_size;
    }

    //
    // Quick return if possible, before checking for invalid pointers.
    //
    if(!m || !n)
    {
        return rocsparse_status_success;
    }

    //
    // Check invalid pointers.
    //
    if(nullptr == descr || nullptr == A || nullptr == csx_row_col_ptr || nullptr == csx_col_row_ind
       || nullptr == csx_val)
    {
        return rocsparse_status_invalid_pointer;
    }

    //
    // Check the description type of the matrix.
    //
    if(rocsparse_matrix_type_general != descr->type)
    {
        return rocsparse_status_not_implemented;
    }

    //
    // Set memory to zero.
    //
    hipMemset2DAsync(A, lda * sizeof(T), 0, m * sizeof(T), n, handle->stream);

    //
    // Compute the conversion.
    //
    return rocsparse_csx2dense_template<DIRA>(
        handle, m, n, descr, csx_val, csx_row_col_ptr, csx_col_row_ind, A, lda);
}

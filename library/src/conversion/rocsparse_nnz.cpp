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
#include "rocsparse_nnz.hpp"
#include "rocsparse.h"

#include "definitions.h"

#include "utility.h"

#include <hip/hip_runtime.h>
#include <rocprim/rocprim.hpp>

template <typename T>
rocsparse_status rocsparse_nnz_impl(rocsparse_handle          handle,
                                    rocsparse_direction       dir,
                                    rocsparse_int             m,
                                    rocsparse_int             n,
                                    const rocsparse_mat_descr descr,
                                    const T*                  A,
                                    rocsparse_int             ld,
                                    rocsparse_int*            nnz_per_row_columns,
                                    rocsparse_int*            nnz_total_dev_host_ptr)
{
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
              "rocsparse_nnz",
              dir,
              m,
              n,
              descr,
              (const void*&)A,
              ld,
              (const void*&)nnz_per_row_columns,
              (const void*&)nnz_total_dev_host_ptr);

    log_bench(
        handle, "./rocsparse_bench", "-f", "nnz", "--dir", dir, "-m", m, "-n", n, "--denseld", ld);

    //
    // Check validity of the direction.
    //
    if(rocsparse_direction_row != dir && rocsparse_direction_column != dir)
    {
        return rocsparse_status_invalid_value;
    }

    //
    // Check sizes
    //
    if((m < 0) || (n < 0) || (ld < m))
    {
        return rocsparse_status_invalid_size;
    }

    //
    // Quick return if possible, before checking for invalid pointers.
    //
    if(!m || !n)
    {

        if(nullptr != nnz_total_dev_host_ptr)
        {
            rocsparse_pointer_mode mode;
            rocsparse_status       status = rocsparse_get_pointer_mode(handle, &mode);
            if(rocsparse_status_success != status)
            {
                return status;
            }

            if(rocsparse_pointer_mode_device == mode)
            {
                RETURN_IF_HIP_ERROR(hipMemsetAsync(
                    nnz_total_dev_host_ptr, 0, sizeof(rocsparse_int), handle->stream));
            }
            else
            {
                *nnz_total_dev_host_ptr = 0;
            }
        }

        return rocsparse_status_success;
    }

    //
    // Check invalid pointers.
    //
    if(nullptr == descr || nullptr == nnz_per_row_columns || nullptr == A
       || nullptr == nnz_total_dev_host_ptr)
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
    // Count.
    //
    {
        rocsparse_status status
            = rocsparse_nnz_template(handle, dir, m, n, A, ld, nnz_per_row_columns);
        if(status != rocsparse_status_success)
        {
            return status;
        }
    }

    //
    // Compute the total number of non-zeros.
    //
    {
        rocsparse_int mn = dir == rocsparse_direction_row ? m : n;
        auto          op = rocprim::plus<rocsparse_int>();
        size_t        temp_storage_size_bytes;
        RETURN_IF_HIP_ERROR(rocprim::reduce(nullptr,
                                            temp_storage_size_bytes,
                                            nnz_per_row_columns,
                                            nnz_total_dev_host_ptr,
                                            0,
                                            mn,
                                            op,
                                            handle->stream));
        temp_storage_size_bytes += sizeof(rocsparse_int);
        bool  temp_alloc       = false;
        void* temp_storage_ptr = nullptr;

        //
        // Device buffer should be sufficient for rocprim in most cases
        //
        rocsparse_int* d_nnz;
        if(handle->buffer_size >= temp_storage_size_bytes)
        {
            d_nnz            = (rocsparse_int*)handle->buffer;
            temp_storage_ptr = d_nnz + 1;
            temp_alloc       = false;
        }
        else
        {
            RETURN_IF_HIP_ERROR(hipMalloc(&d_nnz, temp_storage_size_bytes));
            temp_storage_ptr = d_nnz + 1;
            temp_alloc       = true;
        }

        //
        // Perform reduce
        //
        RETURN_IF_HIP_ERROR(rocprim::reduce(temp_storage_ptr,
                                            temp_storage_size_bytes,
                                            nnz_per_row_columns,
                                            d_nnz,
                                            0,
                                            mn,
                                            op,
                                            handle->stream));

        //
        // Extract nnz
        //
        if(handle->pointer_mode == rocsparse_pointer_mode_device)
        {
            RETURN_IF_HIP_ERROR(hipMemcpyAsync(nnz_total_dev_host_ptr,
                                               d_nnz,
                                               sizeof(rocsparse_int),
                                               hipMemcpyDeviceToDevice,
                                               handle->stream));
        }
        else
        {
            RETURN_IF_HIP_ERROR(hipMemcpy(
                nnz_total_dev_host_ptr, d_nnz, sizeof(rocsparse_int), hipMemcpyDeviceToHost));
        }

        //
        // Free rocprim buffer, if allocated
        //
        if(temp_alloc)
        {
            RETURN_IF_HIP_ERROR(hipFree(d_nnz));
        }
    }

    return rocsparse_status_success;
}

extern "C" {

//
// Check if the macro CAPI_IMPL already exists.
//
#ifdef CAPI_IMPL
#error macro CAPI_IMPL is already defined.
#endif

//
// Definition of the C-implementation.
//
#define CAPI_IMPL(name_, type_)                                                                \
    rocsparse_status name_(rocsparse_handle          handle,                                   \
                           rocsparse_direction       dir,                                      \
                           rocsparse_int             m,                                        \
                           rocsparse_int             n,                                        \
                           const rocsparse_mat_descr descr,                                    \
                           const type_*              A,                                        \
                           rocsparse_int             ld,                                       \
                           rocsparse_int*            nnz_per_row_columns,                      \
                           rocsparse_int*            nnz_total_dev_host_ptr)                   \
    {                                                                                          \
        try                                                                                    \
        {                                                                                      \
            return rocsparse_nnz_impl<type_>(                                                  \
                handle, dir, m, n, descr, A, ld, nnz_per_row_columns, nnz_total_dev_host_ptr); \
        }                                                                                      \
        catch(...)                                                                             \
        {                                                                                      \
            return exception_to_rocsparse_status();                                            \
        }                                                                                      \
    }

//
// C-implementations.
//
CAPI_IMPL(rocsparse_snnz, float);
CAPI_IMPL(rocsparse_dnnz, double);
CAPI_IMPL(rocsparse_cnnz, rocsparse_float_complex);
CAPI_IMPL(rocsparse_znnz, rocsparse_double_complex);

//
// Undefine the macro CAPI_IMPL.
//
#undef CAPI_IMPL
}

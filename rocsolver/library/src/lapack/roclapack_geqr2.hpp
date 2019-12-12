/* ************************************************************************
 * Derived from the BSD2-licensed
 * LAPACK routine (version 3.8) --
 *     Univ. of Tennessee, Univ. of California Berkeley and NAG Ltd..
 *     December 2016
 * Copyright 2018-2019 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#ifndef ROCLAPACK_GEQR2_H
#define ROCLAPACK_GEQR2_H

#include "../auxiliary/rocauxiliary_larf.hpp"
#include "../auxiliary/rocauxiliary_larfg.hpp"
#include "definitions.h"
#include "helpers.h"
#include "ideal_sizes.hpp"
#include "rocblas.hpp"
#include "rocsolver.h"
#include "utility.h"
#include <hip/hip_runtime.h>

template <typename T, typename U>
__global__ void set_one_diag(T* diag, U A, const rocblas_int shifta, const rocblas_int stridea)
{
    int b = hipBlockIdx_x;

    T* d    = load_ptr_batch<T>(A, b, shifta, stridea);
    diag[b] = d[0];
    d[0]    = T(1);
}

template <typename T, typename U>
__global__ void restore_diag(T* diag, U A, const rocblas_int shifta, const rocblas_int stridea)
{
    int b = hipBlockIdx_x;

    T* d = load_ptr_batch<T>(A, b, shifta, stridea);

    d[0] = diag[b];
}

template <typename T, typename U>
rocblas_status rocsolver_geqr2_template(rocblas_handle    handle,
                                        const rocblas_int m,
                                        const rocblas_int n,
                                        U                 A,
                                        const rocblas_int shiftA,
                                        const rocblas_int lda,
                                        rocblas_int const strideA,
                                        T*                ipiv,
                                        const rocblas_int strideP,
                                        const rocblas_int batch_count)
{
    // quick return
    if(m == 0 || n == 0 || batch_count == 0)
        return rocblas_status_success;

    hipStream_t stream;
    rocblas_get_stream(handle, &stream);

    //memory in GPU (workspace)
    T* diag;
    hipMalloc(&diag, sizeof(T) * batch_count);

    rocblas_int dim = min(m, n); //total number of pivots

    for(rocblas_int j = 0; j < dim; ++j)
    {
        // generate Householder reflector to work on column j
        rocsolver_larfg_template(handle,
                                 m - j, //order of reflector
                                 A,
                                 shiftA + idx2D(j, j, lda), //value of alpha
                                 A,
                                 shiftA + idx2D(min(j + 1, m - 1), j, lda), //vector x to work on
                                 1,
                                 strideA, //inc of x
                                 (ipiv + j),
                                 strideP, //tau
                                 batch_count);

        // insert one in A(j,j) tobuild/apply the householder matrix
        hipLaunchKernelGGL(set_one_diag,
                           dim3(batch_count, 1, 1),
                           dim3(1, 1, 1),
                           0,
                           stream,
                           diag,
                           A,
                           shiftA + idx2D(j, j, lda),
                           strideA);

        // Apply Householder reflector to the rest of matrix from the left
        if(j < n - 1)
        {
            rocsolver_larf_template(handle,
                                    rocblas_side_left, //side
                                    m - j, //number of rows of matrix to modify
                                    n - j - 1, //number of columns of matrix to modify
                                    A,
                                    shiftA + idx2D(j, j, lda), //householder vector x
                                    1,
                                    strideA, //inc of x
                                    (ipiv + j),
                                    strideP, //householder scalar (alpha)
                                    A,
                                    shiftA + idx2D(j, j + 1, lda), //matrix to work on
                                    lda,
                                    strideA, //leading dimension
                                    batch_count);
        }

        // restore original value of A(j,j)
        hipLaunchKernelGGL(restore_diag,
                           dim3(batch_count, 1, 1),
                           dim3(1, 1, 1),
                           0,
                           stream,
                           diag,
                           A,
                           shiftA + idx2D(j, j, lda),
                           strideA);
    }

    hipFree(diag);

    return rocblas_status_success;
}

#endif /* ROCLAPACK_GEQR2_H */
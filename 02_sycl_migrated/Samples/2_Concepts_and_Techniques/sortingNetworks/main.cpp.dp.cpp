/* Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * This sample implements bitonic sort and odd-even merge sort, algorithms
 * belonging to the class of sorting networks.
 * While generally subefficient on large sequences
 * compared to algorithms with better asymptotic algorithmic complexity
 * (i.e. merge sort or radix sort), may be the algorithms of choice for sorting
 * batches of short- or mid-sized arrays.
 * Refer to the excellent tutorial by H. W. Lang:
 * http://www.iti.fh-flensburg.de/lang/algorithmen/sortieren/networks/indexen.htm
 *
 * Victor Podlozhnyuk, 07/09/2009
 */

// CUDA Runtime
#include <sycl/sycl.hpp>
using namespace sycl;

// Utilities and system includes
#include <helper_cuda.h>
#include <helper_timer.h>

#include "sortingNetworks_common.h"

////////////////////////////////////////////////////////////////////////////////
// Test driver
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) try {
  
  printf("%s Starting...\n\n", argv[0]);

  printf("Starting up CUDA context...\n");
   sycl::queue q = sycl::queue(default_selector_v);
   std::cout << "\nRunning on " << q.get_device().get_info<info::device::name>()
            << "\n";

  uint *h_InputKey, *h_InputVal, *h_OutputKeyGPU, *h_OutputValGPU;
  uint *d_InputKey, *d_InputVal, *d_OutputKey, *d_OutputVal;
  StopWatchInterface *hTimer = NULL;

  const uint N = 1048576;
  const uint DIR = 0;
  const uint numValues = 65536;
  const uint numIterations = 1;

  printf("Allocating and initializing host arrays...\n\n");
  sdkCreateTimer(&hTimer);
  h_InputKey = (uint *)malloc(N * sizeof(uint));
  h_InputVal = (uint *)malloc(N * sizeof(uint));
  h_OutputKeyGPU = (uint *)malloc(N * sizeof(uint));
  h_OutputValGPU = (uint *)malloc(N * sizeof(uint));
  srand(2001);

  for (uint i = 0; i < N; i++) {
    h_InputKey[i] = rand() % numValues;
    h_InputVal[i] = i;
  }

  printf("Allocating and initializing CUDA arrays...\n\n");
 
  d_InputKey = sycl::malloc_device<uint>(N,q);
  
  d_InputVal = sycl::malloc_device<uint>(N,q);

  d_OutputKey = sycl::malloc_device<uint>(N,q);
  
  d_OutputVal = sycl::malloc_device<uint>(N,q);
  
  q.memcpy(d_InputKey, h_InputKey, N * sizeof(uint))
               .wait();
           
  q.memcpy(d_InputVal, h_InputVal, N * sizeof(uint))
               .wait();
  

  int flag = 1;
  printf("Running GPU oddevenMergesort (%u identical iterations)...\n\n",
         numIterations);

  for (uint arrayLength = 64; arrayLength <= N; arrayLength *= 2) {
    printf("Testing array length %u (%u arrays per batch)...\n", arrayLength,
           N / arrayLength);
    
    q.wait_and_throw();
   

    sdkResetTimer(&hTimer);
    sdkStartTimer(&hTimer);
    uint threadCount = 0;

    for (uint i = 0; i < numIterations; i++)
      threadCount = oddEvenMergeSort(d_OutputKey, d_OutputVal, d_InputKey,
                                d_InputVal, N / arrayLength, arrayLength, DIR,q);

    q.wait_and_throw();

    sdkStopTimer(&hTimer);
    printf("Average time: %f ms\n\n",
           sdkGetTimerValue(&hTimer) / numIterations);

    if (arrayLength == N) {
      double dTimeSecs = 1.0e-3 * sdkGetTimerValue(&hTimer) / numIterations;
      printf(
          "sortingNetworks-oddevenmergesort, Throughput = %.4f MElements/s, Time = %.5f "
          "s, Size = %u elements, NumDevsUsed = %u, Workgroup = %u\n",
          (1.0e-6 * (double)arrayLength / dTimeSecs), dTimeSecs, arrayLength, 1,
          threadCount);
    }

    printf("\nValidating the results...\n");
    printf("...reading back GPU results\n");
    
    q.memcpy(h_OutputKeyGPU, d_OutputKey, N * sizeof(uint))
                 .wait();
   
    q.memcpy(h_OutputValGPU, d_OutputVal, N * sizeof(uint))
                 .wait();

    int keysFlag =
        validateSortedKeys(h_OutputKeyGPU, h_InputKey, N / arrayLength,
                           arrayLength, numValues, DIR);
    int valuesFlag = validateValues(h_OutputKeyGPU, h_OutputValGPU, h_InputKey,
                                    N / arrayLength, arrayLength);
    flag = flag && keysFlag && valuesFlag;

    printf("\n");
  }

  printf("Shutting down...\n");
  sdkDeleteTimer(&hTimer);
  sycl::free(d_OutputVal, q);
  sycl::free(d_OutputKey, q);
  sycl::free(d_InputVal, q);
  sycl::free(d_InputKey, q);
  free(h_OutputValGPU);
  free(h_OutputKeyGPU);
  free(h_InputVal);
  free(h_InputKey);

  exit(flag ? EXIT_SUCCESS : EXIT_FAILURE);
}
catch (sycl::exception const &exc) {
  std::cerr << exc.what() << "Exception caught at file:" << __FILE__
            << ", line:" << __LINE__ << std::endl;
  std::exit(1);
}

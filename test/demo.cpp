#include <iostream>
#include "dispatch.h"

#include <immintrin.h>

void vadd_avx512(int * a, int * b, int * c, int size)
{
	int size_pend = (size / 16) * 16;
	//body
	__m512i va;
	__m512i vb;
	__m512i vc;
	__mmask16 msk = 0xFFFF;
	__m512i src = _mm512_setzero_epi32();
	for(int i = 0; i < size_pend; i+=16)
	{
		//not defined in g++ header file
		//va = _mm512_loadu_epi32(a + i);	
		//vb = _mm512_loadu_epi32(b + i);	
		va = _mm512_mask_loadu_epi32(src, msk, a + i);	
		vb = _mm512_mask_loadu_epi32(src, msk, b + i);	
		vc = _mm512_add_epi32(va, vb);
		//_mm512_storeu_epi32(c + i, vc);
		_mm512_mask_storeu_epi32(c + i, msk, vc);
	}

	//tail
	for(int i = size_pend; i < size; i++)
		c[i] = a[i] + b[i];

	return;
}

void vadd_avx2(int * a, int * b, int * c, int size)
{
	int size_pend = (size / 8) * 8;
	//body
	__m256i va;
	__m256i vb;
	__m256i vc;
	for(int i = 0; i < size_pend; i+=8)
	{
		va = _mm256_loadu_si256((__m256i*)(a + i));	
		vb = _mm256_loadu_si256((__m256i*)(b + i));	
		vc = _mm256_add_epi32(va, vb);
		_mm256_storeu_si256((__m256i*)(c + i), vc);
	}

	//tail
	for(int i = size_pend; i < size; i++)
		c[i] = a[i] + b[i];

	return;
}

void vadd_sse4(int * a, int * b, int * c, int size)
{
	int size_pend = (size / 4) * 4;
	//body
	__m128i va;
	__m128i vb;
	__m128i vc;
	for(int i = 0; i < size_pend; i+=4)
	{
		va = _mm_loadu_si128((__m128i*)(a + i));	
		vb = _mm_loadu_si128((__m128i*)(b + i));	
		vc = _mm_add_epi32(va, vb);
		_mm_storeu_si128((__m128i*)(c + i), vc);
	}

	//tail
	for(int i = size_pend; i < size; i++)
		c[i] = a[i] + b[i];

	return;
}

void vadd_novec(int * a, int * b, int * c, int size)
{

	for(int i = 0; i < size; i++)
		c[i] = a[i] + b[i];

	return;
}

void (*vadd) (int * , int *, int *, int);

int main()
{
  int n = 1<<25;
  int *a = new int[n];
  int *b = new int[n];
  int *c = new int[n];
  int *z = new int[n];

  for(int i = 0; i < n; i++)
  {
	a[i] = i*3;
    b[i] = 2*i;
	c[i] = 0;
    z[i] = 0;
  }
	
  int cpuid = GetBitField();
  //std::cout << cpuid << std::endl;

  if(cpuid & HWY_AVX512)
  {
    std::cout << "using AVX512" << std::endl;
	vadd = vadd_avx512;

  }else if(cpuid & HWY_AVX2)
  {
    std::cout << "using AVX2" << std::endl;
	vadd = vadd_avx2;
  }else if(cpuid & HWY_SSE4)
  {  
    std::cout << "using SSE4" << std::endl;
	vadd = vadd_sse4;
  }else
  {
	std::cout << "using novec" << std::endl;
	vadd = vadd_novec;
  }

  vadd(a, b, c, n);

  //validate
  vadd(a, b, z, n);
	
  for(int i = 0; i < n; i++)
    if(c[i] != z[i])
    {
		std::cout << "check failed at " << i <<
                     "z[i]=" << z[i] << ", c[i]=" << c[i] << std::endl;
		return 0;
	}

  std::cout << "check passed!" << std::endl;

  //not fred for efficiency
  
  return 0;
}

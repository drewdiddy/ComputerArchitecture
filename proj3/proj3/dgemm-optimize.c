#include <xmmintrin.h>
#define BLOCKSIZE 32
#define UNROLL (4)

void reorder(int m, int n, float *A, float *C){

	for(int j = 0; j < m; j++){
		for(int k = 0; k < n; k++){
			for(int i = 0; i < m; i++){
				C[i+j*m] += A[i+k*m] * A[j+k*m];
			}
		}
	}
}

void unrolling(int m, int n, float *A, float *C){
	//we need to unroll different amounts depending on how many iterations we have left
	int unroll;
	for (int i = 0; i < m; i++){
		for(int k = 0; k < n; k++){
			for(int j = 0; j < m; j += 4){
				unroll = m - j;
				if(unroll > 3){
					C[i+j*m] += A[i+k*m] * A[j+k*m];
					C[i+(j + 1) * m] += A[i+k*m] * A[(j + 1) + k*m];
					C[i+(j + 2) * m] += A[i+k*m] * A[(j + 2) + k*m];
					C[i+(j + 3) * m] += A[i+k*m] * A[(j + 3) + k*m];
				}
				//depending on how many iterations are left to do, take the appropriate path
				else if(unroll == 3){
					C[i+j*m] += A[i+k*m] * A[j+k*m];
					C[i+(j + 1) * m] += A[i+k*m] * A[(j + 1) + k*m];
					C[i+(j + 2) * m] += A[i+k*m] * A[(j + 2) + k*m];
				}
				else if(unroll == 2){
					C[i+j*m] += A[i+k*m] * A[j+k*m];
					C[i+(j + 1) * m] += A[i+k*m] * A[(j + 1) + k*m];
				}
				else{
					C[i+j*m] += A[i+k*m] * A[j+k*m];
				}
			}
		}
	}
}

void sse(int m, int n, float *A, float*C){

	__m128 a, b, c;
	int i = 0, j = 0, k = 0;

	for(i = 0; i < m; i++){
		for(k = 0; k < n; k++){
			c = _mm_loadu_si32(&C[i+j*m]);
			for(j = 0; j < m; j++){
				b = _mm_loadu_si32(&A[i+k*m]);
				a = _mm_loadu_si32(&A[j+k*m]);
				c = _mm_add_ps(c, _mm_mul_ps(b, a));
				_mm_storeu_si32(&C[i+j*m], c);
			}
		}
	}
}

int min(int a, int b){
	if(a < b)
		return a;
	else
		return b;
}

//https://en.wikipedia.org/wiki/Loop_nest_optimization USED SITE AS REFERENCE
void tiling(int m, int n, float *A, float *C){
	for(int i = 0; i < m; i += BLOCKSIZE){
		for(int k = 0; k < n; k += BLOCKSIZE){
			for(int j = 0; j < m; j += BLOCKSIZE){
				for(int x = i; x < min(m, i + BLOCKSIZE); x++){
					for(int y = k; y < min(n, k + BLOCKSIZE); y++){
						for(int z = j; z < min(m, j + BLOCKSIZE); z++){
							C[x+z*m] += A[x+y*m] * A[z+y*m];
						}
					}
				}
			}		
		}
	}    
}


void dgemm( int m, int n, float *A, float *C )
{

	//unrolling(m,n,A,C);
	//tiling(m,n,A,C);
	//sse(m,n,A,C);
	reorder(m,n,A,C);

 //    for ( int i = m; i < m + BLOCKSIZE; i += UNROLL * 4 )
 //       for ( int j = n; j < n + BLOCKSIZE; j++ ) {
 //          __m256d c[4];
 //         for ( int x = 0; x < UNROLL; x++ )
 //            c[x] = _mm256_load_pd(C + i + x * 4 + j * n);
 //         /*c[x] = C[i][j]*/
 //         for( int k = m; k < m + BLOCKSIZE; k++ )
 //         {
 //            __m256d b = _mm256_broadcast_sd(B + k + j * n);
 //            /*b = B[k][j]*/
 //         for (int x = 0; x < UNROLL; x++)
 //            c[x] = _mm256_add_pd(c[x], /* c[x] += A[i][k]*b */
 //                   _mm256_mul_pd(_mm256_load_pd(A + n * k + x * 4 + i), b));
 //         }

 //         for ( int x = 0; x < UNROLL; x++ )
 //            _mm256_store_pd(C + i + x * 4 + j * n, c[x]);
 //             /* C[i][j] = c[x] */
 //      }
 //   }
// FILL-IN 
}
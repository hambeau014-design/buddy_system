#include "tests/threads/tests.h"
#include "threads/palloc.h" // 메모리 할당 함수 (palloc_get_page 등) 가정
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>

// 가상의 페이지 크기 상수 (운영체제 환경에서 흔히 사용)
#define PAGE_SIZE 4096

void test_bestfit (void) 
{
  palloc_set_mode (PAL_BEST_FIT);
    msg("Best-Fit start");
    
    // 1. 크기가 다른 세 블록 할당 및 해제하여 틈새 생성
    // 틈새 크기: pA(8KB), pB(16KB), pC(4KB)
    void *pA = palloc_get_page (1); // 2 페이지 (8KB)
    void *pB = palloc_get_page (2); // 4 페이지 (16KB)
    void *pC = palloc_get_page (0); // 1 페이지 (4KB)

    if (!pA || !pB || !pC) fail("초기 할당 실패.");
    
    palloc_free_page (pA); // 8KB 틈새
    palloc_free_page (pB); // 16KB 틈새
    palloc_free_page (pC); // 4KB 틈새
    msg("세 개의 틈새(8KB, 16KB, 4KB) 생성 완료.");

    // 2. 핵심 Best-Fit 로직 검증
    // 요청: 4KB (1 페이지)
    // 4KB를 수용할 수 있는 틈새: {8KB}, {16KB}, {4KB}
    // 이 중 가장 작은(Best) 틈새는 {4KB} (pC의 자리)입니다.
    
    void *pD = palloc_get_page (0); // 1 페이지(4KB) 요청

    if (!pD) fail("pD 할당 실패.");
    
    // 검증: pD의 주소가 가장 작은 틈새(pC의 시작 주소)와 같아야 합니다.
    if (pD != pC) {
        fail("Best-Fit 로직 오류: pD가 가장 적합한 틈새(pC의 자리)가 아닌 곳에 할당됨.");
    }
    
    msg("Best-Fit 로직 검증 성공: pD가 가장 적합한 틈새(pC의 주소)에 할당됨.");

    palloc_free_page(pD); 
    pass();
}

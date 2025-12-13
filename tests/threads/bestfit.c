#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>



void
test_bestfit (void)
{
    palloc_set_mode (PAL_BEST_FIT);
    msg("Best-Fit 모드 설정 완료.");
    
    // 1. 크기가 다른 세 블록 할당 및 해제하여 틈새 생성
    // 틈새 크기: pA(4P), pB(2P), pC(8P)
    void *pA = palloc_get_multiple (PAL_ASSERT, 4); 
    void *pB = palloc_get_multiple (PAL_ASSERT, 2); 
    void *pC = palloc_get_multiple (PAL_ASSERT, 8); 

    msg("A(4P), B(2P), C(8P) 할당 완료.");
    
    palloc_free_multiple (pA, 4); // 4 페이지 틈새
    palloc_free_multiple (pB, 2); // 2 페이지 틈새
    palloc_free_multiple (pC, 8); // 8 페이지 틈새
    msg("세 개의 틈새(4P, 2P, 8P) 생성 완료.");

    // 2. 핵심 Best-Fit 로직 검증
    // 요청: 3 페이지
    // 3페이지를 수용할 수 있는 틈새: {4P}, {8P} (2P는 너무 작음)
    // 이 중 가장 작은(Best) 틈새는 {4P} (pA의 자리)입니다.
    
    void *pD = palloc_get_multiple (PAL_ASSERT, 3); // 3 페이지 요청

    if (!pD) fail("pD 할당 실패.");
    
    // 검증: pD의 주소가 가장 적합한 틈새(pA의 시작 주소)와 같아야 합니다.
    if (pD != pA) {
        // 만약 pD가 pC의 8페이지 틈새에 할당되었다면 Best-Fit 로직 오류입니다.
        fail("Best-Fit 로직 오류: pD가 가장 적합한 틈새(pA의 자리)가 아닌 곳에 할당됨.");
    }
    
    msg("Best-Fit 로직 검증 성공: pD가 pA의 주소에 할당됨.");

    palloc_free_multiple(pD, 3); 
    palloc_free_multiple(pB, 2); 
    palloc_free_multiple(pC, 8);
    pass();
}

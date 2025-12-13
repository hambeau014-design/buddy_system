#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_buddy (void)
{
    palloc_set_mode (PAL_BUDDY);
    msg("Buddy System 모드 설정 완료.");

    // --- 1. 버디 합병 (Coalescing) 검증 ---
    // 4 페이지(Order 2) 크기의 두 버디 블록을 할당하고 해제하여 합병 검증
    
    // palloc_get_multiple(3) 요청 시 2^2=4 페이지가 할당된다고 가정
    void *pA = palloc_get_multiple (PAL_ASSERT, 3); // 4 페이지 할당 (Order 2)
    void *pB = palloc_get_multiple (PAL_ASSERT, 3); // 4 페이지 할당 (Order 2)

    if (!pA || !pB) fail("버디 블록 할당 실패.");
    msg("두 개의 버디 블록 pA(4P), pB(4P) 할당 완료.");

    // B 해제 (버디 합병 대기)
    palloc_free_multiple (pB, 4); // Buddy System은 4페이지 크기로 해제해야 함
    msg("pB 해제.");
    
    // A 해제: pA와 pB는 이제 합쳐져 8 페이지 블록(Order 3)이 되어야 합니다.
    palloc_free_multiple (pA, 4); 
    msg("pA 해제. pA와 pB가 성공적으로 합병되어 8페이지 틈새가 되었는지 검증.");

    // 2. 검증: 8 페이지 요청을 시도하여, 합병된 공간에 할당되는지 확인
    void *pC = palloc_get_multiple (PAL_ASSERT, 8); // 8 페이지 요청 (Order 3)
    
    if (!pC) fail("합병된 8 페이지 공간 요청 실패. 합병 오류 가능성.");
    
    // pC가 해제된 버디 블록 중 하나인 pA의 주소와 같아야 합병 후 재사용되었음을 의미
    if (pC != pA) {
        fail("Buddy System 로직 오류: 합병된 블록이 재사용되지 않았거나 합병 실패 (pA의 주소에 할당되어야 함).");
    }
    
    msg("Buddy System 합병 로직 검증 성공: pA, pB가 합병되어 8페이지 블록으로 재사용됨.");

    palloc_free_multiple(pC, 8); 
    pass();
}

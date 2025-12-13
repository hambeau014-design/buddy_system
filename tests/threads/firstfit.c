#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_firstfit (void)
{
    // First-Fit 모드 설정
    palloc_set_mode (PAL_FIRST_FIT);
    msg("First-Fit Start");
    
    // 1. 크기가 다른 세 블록 할당 (1P, 2P, 1P)
    void *pA = palloc_get_multiple (PAL_ASSERT, 1);
    void *pB = palloc_get_multiple (PAL_ASSERT, 2);
    void *pC = palloc_get_multiple (PAL_ASSERT, 1);

    msg("|A(1P)| B(2P) | C(1P)| 할당");
    
    // 2. 중앙 블록 B 해제하여 틈새 생성: A [사용] -> B [여유 2P] -> C [사용]
    palloc_free_multiple (pB, 2);
    msg("블록 B (2페이지) 해제====| A(1P) |     | C(1P)|");

    // 3. First-Fit 핵심 테스트
    // 요청: 1 페이지
    void *pD = palloc_get_multiple (PAL_ASSERT, 1); 

    if (pD != pB) {
        // 만약 pD가 pC 뒤의 다른 공간에 할당되었다면 오류
        fail("First-Fit 로직 오류: 첫 번째 틈새(pB의 자리)에 할당되지 않음.");
    }
    
    msg("First-Fit 로직 검증 성공: pB의 자리에 할당됨====| A(1P) | D(1P) | C(1P) |");
    
    palloc_free_multiple(pA, 1); 
    palloc_free_multiple(pC, 1); 
    palloc_free_multiple(pD, 1);
    pass();
}

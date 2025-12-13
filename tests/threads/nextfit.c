#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_nextfit (void)
{
    palloc_set_mode (PAL_NEXT_FIT);
    msg("Next-Fit 모드 설정 완료.");

    // 1. A, B, C 할당 (순서대로 주소가 증가한다고 가정)
    void *pA = palloc_get_multiple (PAL_ASSERT, 1); 
    void *pB = palloc_get_multiple (PAL_ASSERT, 1); 
    void *pC = palloc_get_multiple (PAL_ASSERT, 1); 

    // 2. A와 C 해제: A [여유] -> B [사용] -> C [여유]
    palloc_free_multiple (pA, 1); // 리스트의 초반 틈새 (First-Fit이라면 여기에 할당)
    palloc_free_multiple (pC, 1); // 리스트의 후반 틈새

    // 3. 임시 할당 (pD): B의 다음 위치에 할당되어 next_idx를 리스트 후반으로 이동시킵니다.
    void *pD = palloc_get_multiple (PAL_ASSERT, 1);
    if (!pD) fail("pD 할당 실패.");
    msg("pD 할당 완료. Next Scan은 pD 이후부터 시작됨.");

    // 4. Next-Fit 핵심 테스트
    // 요청: 1 페이지. 
    // Next-Fit은 pD 이후부터 검색을 시작하여 순환 검색을 통해 pA 틈새를 찾아야 합니다.
    void *pE = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pE) fail("pE 할당 실패.");

    // 검증: pE가 pA (리스트 초반)에 할당되었는지 확인합니다.
    if (pE != pA) {
        fail("Next-Fit 로직 오류: 순환 검색을 통해 pA 틈새를 찾지 못함.");
    }
    
    msg("Next-Fit 로직 검증 성공: pE가 pA의 틈새에 순환하여 할당됨.");

    palloc_free_multiple(pB, 1); 
    palloc_free_multiple(pC, 1); 
    palloc_free_multiple(pD, 1);
    palloc_free_multiple(pE, 1);
    pass();
}

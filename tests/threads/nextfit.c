#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_nextfit (void)
{
    palloc_set_mode (PAL_NEXT_FIT);
    msg("Next-Fit 모드 설정 완료.");

    // 1. A, B 할당 및 해제: A [여유 1P] -> B [사용 1P]
    // (A를 먼저 해제하여 검색 순환을 유도할 수 있는 틈새를 만듭니다.)
    void *pA = palloc_get_multiple (PAL_ASSERT, 1);
    void *pB = palloc_get_multiple (PAL_ASSERT, 1);
    
    palloc_free_multiple (pA, 1); // 1페이지 틈새 (리스트의 초반에 위치)
    msg("A (1P) 해제: 초반에 틈새 생성.");
    
    // 2. C 할당 (pB 다음의 새로운 공간에 할당된다고 가정)
    // 이 할당으로 인해 next_idx가 C 이후로 이동합니다.
    void *pC = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pC) fail("pC 할당 실패.");
    msg("pC 할당 완료. 다음 검색은 pC 이후부터 시작됨.");

    // 3. Next-Fit 핵심 테스트
    // 요청: 1 페이지
    // Next-Fit은 pC 이후부터 검색을 시작하여 리스트 끝까지 간 후, 
    // 순환하여 pA의 틈새를 찾아야 합니다.
    
    void *pD = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pD) fail("pD 할당 실패.");

    // 검증: pD가 pC 다음에 있는 공간이 아닌, pA의 틈새에 할당되었는지 확인합니다.
    // (만약 First-Fit이었다면 C 다음의 새 공간 대신 pA 틈새에 할당되었을 것입니다.)
    // Next-Fit의 '순환 후 재사용'을 검증합니다.
    
    if (pD != pA) {
        // pD가 pA에 할당되지 않았다면, next_idx 업데이트 로직 또는 순환 로직에 오류가 있을 수 있습니다.
        fail("Next-Fit 로직 오류: 순환 검색 후에도 pA의 틈새를 찾지 못함.");
    }
    
    msg("Next-Fit 로직 검증 성공: pD가 순환하여 pA의 틈새에 할당됨.");

    palloc_free_multiple(pB, 1); 
    palloc_free_multiple(pC, 1); 
    palloc_free_multiple(pD, 1);
    pass();
}


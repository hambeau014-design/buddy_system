#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_nextfit (void)
{
    palloc_set_mode (PAL_NEXT_FIT);
    msg("Next-Fit 모드 설정 완료.");

    // 1. 초기 조각화: pA (앞쪽 틈새), pB (중앙 할당), pC (뒷쪽 틈새)
    void *pA_alloc = palloc_get_multiple (PAL_ASSERT, 2); // A 할당
    void *pB = palloc_get_multiple (PAL_ASSERT, 4);      // B 할당
    void *pC_alloc = palloc_get_multiple (PAL_ASSERT, 2); // C 할당

    // A를 해제하여 초반에 틈새를 만듭니다. (next_idx는 pC 뒤에 위치)
    palloc_free_multiple (pA_alloc, 2);
    msg("A(2P) 해제: 초반에 틈새 생성됨.");
    
    // 2. Next-Fit 핵심 테스트: 순환 검증
    // 요청: 1 페이지
    // next_idx는 pC 뒤에 있으므로, pC 뒤부터 검색을 시작하여 리스트 끝까지 간 후,
    // 순환하여 pA의 틈새를 찾아야 합니다.
    
    void *pD = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pD) fail("pD 할당 실패.");

    // 검증: pD가 pA (첫 번째 틈새였던 곳)에 할당되었는지 확인합니다.
    // 만약 First-Fit이었다면 pA의 자리가 아닌 새로운 공간에 할당될 수도 있습니다.
    // Next-Fit의 순환 검색을 통해 pD가 pA의 틈새에 할당되어야 합니다.
    
    if (pD != pA_alloc) {
        fail("Next-Fit 로직 오류: 순환 검색 후에도 pA의 틈새를 찾지 못함 (pA의 주소에 할당되어야 함).");
    }
    
    msg("Next-Fit 로직 검증 성공: pD가 pA의 틈새에 순환하여 할당됨.");

    palloc_free_multiple(pB, 4); 
    palloc_free_multiple(pC_alloc, 2); 
    palloc_free_multiple(pD, 1);
    pass();
}

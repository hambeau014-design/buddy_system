#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_nextfit (void)
{
    palloc_set_mode (PAL_NEXT_FIT);
    msg("Next-Fit 모드 설정 완료.");

    // 1. 초기 조각화: A (초반 틈새), B (중앙 할당), C (후반 할당)
    void *pA_alloc = palloc_get_multiple (PAL_ASSERT, 2); // 1. A 할당
    void *pB = palloc_get_multiple (PAL_ASSERT, 4);      // 2. B 할당
    void *pC = palloc_get_multiple (PAL_ASSERT, 2);      // 3. C 할당 (next_idx는 C 끝에 위치)

    // A를 해제하여 초반에 틈새를 만듭니다. (First-Fit이라면 여기에 할당될 것)
    palloc_free_multiple (pA_alloc, 2);
    msg("A(2P) 해제: 리스트 초반에 틈새 생성됨.");
    
    // 2. Next-Fit 핵심 테스트: 순환 검증
    // 요청: 1 페이지. 
    // Next-Fit은 C 끝부터 검색을 시작하여, A 틈새를 건너뛰고 B 뒤쪽의 공간을 먼저 탐색합니다.
    // (메모리 맵에 따라) A 틈새를 건너뛰고 리스트 끝까지 간 후, 순환 검색을 통해 
    // 다시 A 틈새를 찾아 할당하는 시나리오를 기대합니다.
    
    void *pD = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pD) fail("pD 할당 실패.");

    // 검증: pD가 pA (첫 번째 틈새였던 곳)에 할당되었는지 확인합니다.
    // pD가 pA의 주소와 같다면, Next-Fit이 순환 검색을 통해 A를 재사용했음을 의미합니다.
    if (pD != pA_alloc) {
        fail("Next-Fit 로직 오류: 검색 시작점(C 이후)을 지켰음에도 순환 후 A에 할당되지 않음.");
    }
    
    msg("Next-Fit 로직 검증 성공: pD가 pA의 틈새에 순환하여 할당됨.");

    palloc_free_multiple(pB, 4); 
    palloc_free_multiple(pC, 2); 
    palloc_free_multiple(pD, 1);
    pass();
}

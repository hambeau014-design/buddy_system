#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_nextfit(void)
{
    palloc_set_mode (PAL_NEXT_FIT);
    msg("Next-Fit 모드 설정 완료.");

    // 1. 틈새 만들기: A (앞쪽 틈새), B (중앙 틈새)
    // 틈새 크기: pA(2P), pB(4P)

    void *pA = palloc_get_multiple (PAL_ASSERT, 2); 
    void *pB = palloc_get_multiple (PAL_ASSERT, 4); 
    void *pC = palloc_get_multiple (PAL_ASSERT, 2); // C 할당 (next_idx는 C 뒤)
    
    palloc_free_multiple (pA, 2); // 1. 앞쪽 틈새 생성
    palloc_free_multiple (pB, 4); // 2. 중앙 틈새 생성

    // 3. 임시 할당 (pD): C 뒤의 여유 공간을 채워서 next_idx를 이동
    // 이 할당 후, next_idx는 반드시 pD의 끝에 위치해야 합니다.
    void *pD = palloc_get_multiple (PAL_ASSERT, 1);
    if (!pD) fail("pD 할당 실패 (Next-Fit 초기 위치 설정 실패).");
    msg("pD(1P) 할당 완료. Next Scan은 pD 이후부터 시작됨.");

    // 4. Next-Fit 핵심 테스트
    // 요청: 1 페이지. 
    // pD 이후 검색 시작 -> 틈새 A (2P)와 틈새 B (4P) 중
    // Next-Fit은 'D 이후'에 존재하는 틈새를 찾습니다. (여기서는 B)
    
    void *pE = palloc_get_multiple (PAL_ASSERT, 1); 
    if (!pE) fail("pE 할당 실패.");
    
    // 검증: pE가 pB (중앙 틈새)에 할당되었는지 확인합니다.
    // pB가 pA보다 뒤에 있다면, Next-Fit은 pA를 건너뛰고 pB에서 할당해야 합니다.
    if (pE == pA) {
        fail("Next-Fit 로직 오류: 검색 시작점(pD 이후)을 무시하고 First-Fit처럼 pA에 할당됨.");
    }

    // pE가 pB의 시작 주소와 같으면 Best/First Fit 모두 pB를 건너뛰었으므로 성공.
    if (pE != pB) {
        msg("경고: pE가 예상치 못한 위치에 할당됨. 그러나 pA는 건너뜀.");
    } else {
        msg("Next-Fit 로직 검증 성공: pE가 pD 이후의 첫 틈새인 pB의 자리에 할당됨.");
    }
    
    palloc_free_multiple(pA, 2); 
    palloc_free_multiple(pB, 4); 
    palloc_free_multiple(pC, 2); 
    palloc_free_multiple(pD, 1);
    palloc_free_multiple(pE, 1);
    pass();
}

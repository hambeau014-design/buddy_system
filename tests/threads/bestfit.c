#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void test_bestfit (void) 
{
  palloc_set_mode (PAL_BEST_FIT);
  // Best-Fit 모드 설정
    palloc_set_mode (PAL_BEST_FIT);
    msg("Best-Fit 모드로 설정 완료.");

    // 필요한 메모리 크기 (페이지 Order 기준)
    // Order 0: 1 페이지 (가장 작은 크기)
    // Order 1: 2 페이지
    // Order 2: 4 페이지
    // Order 3: 8 페이지

    // --- 1. 초기 조각화: 크기가 다른 세 틈새 생성 ---
    
    // A. 4 페이지 블록 할당 (Order 2)
    void *pA = palloc_get_page (2);
    // B. 8 페이지 블록 할당 (Order 3)
    void *pB = palloc_get_page (3);
    // C. 2 페이지 블록 할당 (Order 1)
    void *pC = palloc_get_page (1);

    if (pA == NULL || pB == NULL || pC == NULL) {
        fail("초기 할당 실패: 테스트 실행 불가.");
    }
    msg("크기: 4, 8, 2 페이지 블록 할당 완료.");
    
    // 할당된 블록을 해제하여 틈새(여유 공간)를 만듭니다.
    // 메모리 맵의 순서는 A -> B -> C 순이라고 가정합니다.
    
    // 틈새 크기: A(4), B(8), C(2)
    palloc_free_page (pA); // 4 페이지 틈새 생성
    palloc_free_page (pB); // 8 페이지 틈새 생성
    palloc_free_page (pC); // 2 페이지 틈새 생성
    msg("세 개의 틈새(4, 8, 2 페이지) 생성 완료.");

    // --- 2. 핵심 Best-Fit 로직 검증 ---
    
    // 요청 크기: 2^2 = 4 페이지 (Order 2)
    // 여유 공간: {4 페이지}, {8 페이지}, {2 페이지}
    // Best-Fit은 4 페이지를 수용할 수 있는 가장 작은 틈새를 찾아야 합니다.
    // 4 페이지를 수용할 수 있는 틈새: {4 페이지} (A의 자리)와 {8 페이지} (B의 자리)
    // 이 중 가장 작은 틈새는 {4 페이지} (A의 자리)입니다.
    
    // 따라서, 새로 할당된 pD는 pA의 시작 주소와 같아야 합니다.

    void *pD = palloc_get_page (2); // 4 페이지 블록 요청

    if (pD == NULL) {
        fail("4 페이지 요청에 실패. 여유 공간 부족?");
    }
    msg("4 페이지 블록 pD 요청 완료.");

    // 검증: pD가 pA (가장 적합한 크기)의 자리에 할당되었는지 확인합니다.
    if (pD != pA) {
        /*
         Best-Fit은 4페이지를 수용 가능한 최소 크기인 pA의 4페이지 틈새를 선택해야 합니다.
         만약 pD가 pB의 8페이지 틈새에서 할당되었다면 Best-Fit 로직 오류입니다.
        */
        fail("Best-Fit 로직 오류: pD가 가장 작은 틈새(pA의 자리)가 아닌 다른 곳에 할당됨.");
    }
    
    msg("Best-Fit 로직 검증 성공: pD가 가장 적합한 틈새(pA의 주소)에 할당됨.");

    // --- 3. 마무리 및 정리 ---
    // 할당된 pD를 해제합니다.
    palloc_free_page (pD); 
    
    // 모든 테스트가 성공적으로 완료되었을 때만 호출
    pass();
}

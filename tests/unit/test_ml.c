#include "glk_svc/glk_ml.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    assert(glk_ml_init() == GLK_OK);
    int32_t pulses[] = {3, 12, 40, 8};
    float feats[8];
    size_t n = glk_ml_features_from_pulses(pulses, 4, feats, 8);
    assert(n >= 3);
    glk_ml_result_t r;
    memset(&r, 0, sizeof(r));
    assert(glk_ml_infer(feats, n, &r) == GLK_OK);
    assert(r.label >= 0);
    printf("test_ml: OK label=%d score=%.2f\n", r.label, r.score);
    return 0;
}

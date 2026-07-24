#include "glk_svc/glk_ml.h"
#include "glk/glk_config.h"

#include <string.h>
#include <stdlib.h>

#if GLK_ML_MODEL_MAX > 0
static uint8_t s_model[GLK_ML_MODEL_MAX];
#else
static uint8_t s_model[1];
#endif
static size_t s_model_len;
static bool s_loaded;
#if GLK_ML_ARENA_SIZE > 0
static uint8_t s_arena[GLK_ML_ARENA_SIZE];
#else
static uint8_t s_arena[1];
#endif

glk_err_t glk_ml_init(void) {
    s_model_len = 0;
    s_loaded = false;
    memset(s_arena, 0, sizeof(s_arena));
    (void)s_arena;
    return GLK_OK;
}

size_t glk_ml_features_from_pulses(const int32_t* pulses, size_t n, float* out, size_t out_n) {
    if (!pulses || !out || out_n == 0) return 0;
    float sum = 0, mx = 0;
    for (size_t i = 0; i < n; i++) {
        float v = (float)pulses[i];
        sum += v;
        if (v > mx) mx = v;
    }
    size_t w = 0;
    if (w < out_n) out[w++] = n ? sum / (float)n : 0;
    if (w < out_n) out[w++] = mx;
    if (w < out_n) out[w++] = sum;
    /* coarse histogram into remaining bins */
    for (size_t b = 0; b + w < out_n && b < 5; b++) {
        int cnt = 0;
        float lo = (float)b * 10.f;
        float hi = lo + 10.f;
        for (size_t i = 0; i < n; i++) {
            if ((float)pulses[i] >= lo && (float)pulses[i] < hi) cnt++;
        }
        out[w++] = (float)cnt;
    }
    return w;
}

glk_err_t glk_ml_infer(const float* features, size_t n, glk_ml_result_t* out) {
    if (!out) return GLK_ERR_INVAL;
    memset(out, 0, sizeof(*out));
    out->model_loaded = s_loaded;
    float f0 = (n && features) ? features[0] : 0.f;
    if (s_loaded && s_model_len > 4) {
        /* Placeholder: checksum fold as pseudo-classifier — real TFLite Micro hooks here */
        uint32_t h = 0;
        for (size_t i = 0; i < s_model_len; i++) h = h * 131u + s_model[i];
        out->label = (int32_t)((h + (uint32_t)f0) % 3u);
        out->score = 0.5f + (float)(out->label) * 0.1f;
    } else {
        /* Heuristic anomaly: high pulse density */
        if (f0 > 25.f) {
            out->label = 1; /* busy */
            out->score = 0.7f;
        } else if (f0 > 10.f) {
            out->label = 0; /* normal */
            out->score = 0.6f;
        } else {
            out->label = 2; /* quiet */
            out->score = 0.65f;
        }
    }
    return GLK_OK;
}

glk_err_t glk_ml_load_model(const void* data, size_t len) {
    if (!data || len == 0 || len > GLK_ML_MODEL_MAX) return GLK_ERR_INVAL;
    memcpy(s_model, data, len);
    s_model_len = len;
    s_loaded = true;
    return GLK_OK;
}

void glk_ml_unload_model(void) {
    s_model_len = 0;
    s_loaded = false;
}

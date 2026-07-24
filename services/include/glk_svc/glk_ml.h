/**
 * Tiny feature extractors + optional TinyML hook.
 * Outputs never auto-trigger TX/GPIO — policy filters all actuation.
 */
#pragma once

#include "glk/glk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t label;
    float score;
    bool model_loaded;
} glk_ml_result_t;

glk_err_t glk_ml_init(void);

/** Extract simple histogram features into out[0..out_n). */
size_t glk_ml_features_from_pulses(const int32_t* pulses, size_t n, float* out, size_t out_n);

/**
 * Run tiny model or heuristic classifier.
 * If no model file loaded, uses threshold heuristic on feature[0].
 */
glk_err_t glk_ml_infer(const float* features, size_t n, glk_ml_result_t* out);

/** Load raw model blob (≤ GLK_ML_MODEL_MAX); optional CMSIS-NN/TFLite later. */
glk_err_t glk_ml_load_model(const void* data, size_t len);
void glk_ml_unload_model(void);

#ifdef __cplusplus
}
#endif

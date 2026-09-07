#include "tinyml.h"
#include "global.h"
#include "neo_blinky.h"

namespace {
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 10 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
}

void setupTinyML() {
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;
    model = tflite::GetModel(dht_anomaly_model_tflite);

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;
    interpreter->AllocateTensors();

    input = interpreter->input(0);
    output = interpreter->output(0);
}

void tiny_ml_task(void *pvParameters) {
    setupTinyML();
    float prev_t = glob_temperature;
    float prev_h = glob_humidity;

    while (1) {
        float delta_t = glob_temperature - prev_t;
        float delta_h = glob_humidity - prev_h;
        prev_t = glob_temperature;
        prev_h = glob_humidity;

        // Truyền 4 tham số vào Tensor đầu vào
        input->data.f[0] = glob_temperature;
        input->data.f[1] = glob_humidity;
        input->data.f[2] = delta_t;
        input->data.f[3] = delta_h;
        
        if (interpreter->Invoke() == kTfLiteOk) {
            // Tìm class có xác suất cao nhất
            int best_class = 0;
            float max_prob = output->data.f[0];
            for (int i = 1; i < 4; i++) {
                if (output->data.f[i] > max_prob) {
                    max_prob = output->data.f[i];
                    best_class = i;
                }
            }
            comfort_class = best_class;
            rgb_display_comfort(comfort_class);
            Serial.printf("AI Result -> Comfort Class: %d (Prob: %.2f) | [0-COLD]: %.2f | [1-COMFORT]: %.2f | [2-WARM]: %.2f | [3-HOT]: %.2f\n",
                          comfort_class,
                          max_prob,
                          output->data.f[0],
                          output->data.f[1],
                          output->data.f[2],
                          output->data.f[3]);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
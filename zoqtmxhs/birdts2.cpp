#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

extern "C" {
#include "tensorflow/lite/c/c_api.h"
}

// 문턱값 설정
#define CONF_THRESH 0.4
#define IOU_THRESH 0.4

int main() {
    printf("프로그램 시작\n");

    // ===== 1. TFLite 모델 초기화 =====
    TfLiteModel* model = TfLiteModelCreateFromFile("float32.tflite");
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, 4);

    TfLiteInterpreter* interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);

    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(interpreter, 0);

    // ===== 2. IP 카메라 연결 =====
    cv::VideoCapture cap("http://192.168.0.17:8080/video");
    if (!cap.isOpened()) {
        printf("IP 카메라 연결 실패\n");
        return -1;
    }

    cv::Mat frame, resized;

    while (1) {
        cap >> frame;
        if (frame.empty()) break;

        // ===== 3. 전처리 =====
        cv::resize(frame, resized, cv::Size(640, 640));

        // 🔥 RGB 변환 (필수)
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);

        resized.convertTo(resized, CV_32F, 1.0 / 255.0);

        float* input_data = (float*)TfLiteTensorData(input);
        memcpy(input_data, resized.data, 640 * 640 * 3 * sizeof(float));

        // ===== 4. 추론 =====
        TfLiteInterpreterInvoke(interpreter);

        // ===== 5. 출력 =====
        const TfLiteTensor* output = TfLiteInterpreterGetOutputTensor(interpreter, 0);
        float* out = (float*)TfLiteTensorData(output);

        std::vector<cv::Rect> bboxes;
        std::vector<float> scores;

        const int num = 8400;

        for (int i = 0; i < num; i++) {

            // 🔥 인덱싱 수정 (핵심)
            float conf = out[4 * num + i];

            if (conf > CONF_THRESH) {
                float x_center = out[0 * num + i];
                float y_center = out[1 * num + i];
                float w        = out[2 * num + i];
                float h        = out[3 * num + i];

                int width  = (int)(w * frame.cols);
                int height = (int)(h * frame.rows);
                int x1     = (int)((x_center - w / 2) * frame.cols);
                int y1     = (int)((y_center - h / 2) * frame.rows);

                bboxes.push_back(cv::Rect(x1, y1, width, height));
                scores.push_back(conf);
            }
        }

        // ===== 6. NMS =====
        std::vector<int> indices;
        cv::dnn::NMSBoxes(bboxes, scores, CONF_THRESH, IOU_THRESH, indices);

        // ===== 7. 결과 표시 =====
        for (size_t i = 0; i < indices.size(); i++) {
            int idx = indices[i];
            cv::Rect box = bboxes[idx];

            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);

            char text[50];
            sprintf(text, "bird %.2f", scores[idx]);

            cv::putText(frame, text, cv::Point(box.x, box.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(0, 255, 0), 2);
        }

        cv::Mat display;
        cv::resize(frame, display, cv::Size(640, 480));
        cv::imshow("YOLO Detection (NMS)", display);

        if (cv::waitKey(1) == 27) break;
    }

    cap.release();
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);

    return 0;
}
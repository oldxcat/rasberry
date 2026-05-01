#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>

extern "C" {
#include "tensorflow/lite/c/c_api.h"
}

#define CONF_THRESH 0.2

struct Box {
    float x1, y1, x2, y2, conf;
};

// IoU 계산
float IoU(const Box& a, const Box& b) {
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);

    float inter = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
    float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
    float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);

    return inter / (areaA + areaB - inter + 1e-6);
}

int main() {
    printf("프로그램 시작\n");

    // ===== 모델 =====
    TfLiteModel* model = TfLiteModelCreateFromFile("best_int8.tflite");
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, 4);

    TfLiteInterpreter* interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);

    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(interpreter, 0);

    // ===== 카메라 =====
    cv::VideoCapture cap("http://192.168.0.17:8080/video");
    printf("카메라 열림 상태: %d\n", cap.isOpened());

    if (!cap.isOpened()) {
        printf("IP 카메라 연결 실패\n");
        return -1;
    }

    cv::Mat frame, resized;

    while (1) {
        cap >> frame;
        printf("frame 들어옴\n");

        if (frame.empty()) {
            printf("frame empty!\n");
            break;
        }

        // ===== 입력 =====
        cv::resize(frame, resized, cv::Size(640, 640));
        resized.convertTo(resized, CV_32F, 1.0/255);

        float* input_data = (float*)TfLiteTensorData(input);
        memcpy(input_data, resized.data, 640*640*3*sizeof(float));

        // ===== 추론 =====
        TfLiteInterpreterInvoke(interpreter);

        // ===== 출력 =====
        const TfLiteTensor* output = TfLiteInterpreterGetOutputTensor(interpreter, 0);

        TfLiteQuantizationParams out_params = TfLiteTensorQuantizationParams(output);
        float out_scale = out_params.scale;
        int out_zero = out_params.zero_point;

        int8_t* out = (int8_t*)TfLiteTensorData(output);

        // 🔥 출력 크기 확인 (중요)
        int total = TfLiteTensorByteSize(output);
        printf("output bytes: %d\n", total);

        // 🔥 클래스 수 추정 (bird = 1로 가정)
        int num_classes = 1;
        int stride = 5 + num_classes;

        std::vector<Box> boxes;

        for (int i = 0; i < 8400; i++) {
            int idx = i * stride;

            float x = (out[idx + 0] - out_zero) * out_scale;
            float y = (out[idx + 1] - out_zero) * out_scale;
            float w = (out[idx + 2] - out_zero) * out_scale;
            float h = (out[idx + 3] - out_zero) * out_scale;

            float obj = (out[idx + 4] - out_zero) * out_scale;

            // 🔥 클래스 confidence
            float max_cls = 0;
            for (int c = 0; c < num_classes; c++) {
                float cls = (out[idx + 5 + c] - out_zero) * out_scale;
                if (cls > max_cls) max_cls = cls;
            }

            float conf = obj * max_cls;

            if (i < 10) {
                printf("obj: %f, cls: %f, conf: %f\n", obj, max_cls, conf);
            }

            if (conf > CONF_THRESH) {
                Box b;
                b.x1 = (x - w/2) * frame.cols;
                b.y1 = (y - h/2) * frame.rows;
                b.x2 = (x + w/2) * frame.cols;
                b.y2 = (y + h/2) * frame.rows;
                b.conf = conf;

                boxes.push_back(b);
            }
        }

        // ===== NMS =====
        std::sort(boxes.begin(), boxes.end(), [](Box a, Box b) {
            return a.conf > b.conf;
        });

        std::vector<Box> final_boxes;
        float iou_thresh = 0.5;

        for (auto& b : boxes) {
            bool keep = true;

            for (auto& fb : final_boxes) {
                if (IoU(b, fb) > iou_thresh) {
                    keep = false;
                    break;
                }
            }

            if (keep) {
                final_boxes.push_back(b);
            }
        }

        // ===== draw =====
        for (auto& b : final_boxes) {
            cv::rectangle(frame,
                          cv::Point(b.x1, b.y1),
                          cv::Point(b.x2, b.y2),
                          cv::Scalar(0,255,0), 1);

            char text[50];
            sprintf(text, "bird %.2f", b.conf);

            cv::putText(frame, text,
                        cv::Point(b.x1, b.y1 - 5),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.5,
                        cv::Scalar(0,255,0), 1);
        }

        // ===== 출력 축소 =====
        cv::Mat display;
        cv::resize(frame, display, cv::Size(320, 240));

        cv::imshow("YOLO IP CAM", display);

        if (cv::waitKey(1) == 27) break;
    }

    cap.release();
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);

    return 0;
}
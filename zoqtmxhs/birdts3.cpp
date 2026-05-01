#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

extern "C" {
#include "tensorflow/lite/c/c_api.h"
}

#define CONF_THRESH 0.5 
#define IOU_THRESH 0.45 

int main() {
    printf("YOLOv8 + Pi Camera Module 3 Start\n");

    // ===== TFLite 모델 =====
    TfLiteModel* model = TfLiteModelCreateFromFile("best_int8.tflite");
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, 4);

    TfLiteInterpreter* interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);

    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(interpreter, 0);
    const TfLiteTensor* output = TfLiteInterpreterGetOutputTensor(interpreter, 0);

    // ===== Pi Camera Module 3 (libcamera + GStreamer) =====
    cv::VideoCapture cap(
        "libcamerasrc ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! "
        "queue leaky=2 max-size-buffers=1 ! "
        "appsink drop=true sync=false",
        cv::CAP_GSTREAMER
    );

    if (!cap.isOpened()) {
        printf("카메라 열기 실패\n");
        return -1;
    }

    cv::Mat frame, resized, rgb, display;

    double fps = 0;

    while (1) {
        double start = (double)cv::getTickCount();

        cap >> frame;
        if (frame.empty()) continue;

        // ===== 전처리 =====
        cv::resize(frame, resized, cv::Size(320, 320));
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

        float* input_ptr = (float*)TfLiteTensorData(input);
        memcpy(input_ptr, rgb.data, 320 * 320 * 3 * sizeof(float));

        // ===== 추론 =====
        TfLiteInterpreterInvoke(interpreter);

        float* out = (float*)TfLiteTensorData(output);
        const int num_elements = 8400;

        std::vector<cv::Rect> bboxes;
        std::vector<float> scores;

        for (int i = 0; i < num_elements; i++) {
            float conf = out[4 * num_elements + i];
            if (conf < CONF_THRESH) continue;

            float x_c = out[0 * num_elements + i];
            float y_c = out[1 * num_elements + i];
            float w   = out[2 * num_elements + i];
            float h   = out[3 * num_elements + i];

            // 픽셀 변환
            int x1 = (int)((x_c - w / 2.0f) * frame.cols);
            int y1 = (int)((y_c - h / 2.0f) * frame.rows);
            int w_px = (int)(w * frame.cols);
            int h_px = (int)(h * frame.rows);

            // 이상값 방지
            if (w_px <= 0 || h_px <= 0) continue;

            bboxes.push_back(cv::Rect(x1, y1, w_px, h_px));
            scores.push_back(conf);
        }

        // ===== NMS =====
        std::vector<int> indices;
        if (!bboxes.empty()) {
            cv::dnn::NMSBoxes(bboxes, scores, CONF_THRESH, IOU_THRESH, indices);
        }

        // ===== 박스 출력 =====
        for (int idx : indices) {
            cv::rectangle(frame, bboxes[idx], cv::Scalar(0, 255, 0), 2);
        }

        // ===== FPS =====
        double end = (double)cv::getTickCount();
        double total_time = (end - start) / cv::getTickFrequency();
        fps = 1.0 / total_time;

        char fps_text[30];
        sprintf(fps_text, "FPS: %.1f", fps);

        cv::putText(frame, fps_text,
                    cv::Point(20, 40),
                    cv::FONT_HERSHEY_SIMPLEX,
                    1.0,
                    cv::Scalar(0, 255, 255),
                    2);

        // ===== 화면 출력 =====
        cv::resize(frame, display, cv::Size(480, 320));
        cv::imshow("YOLOv8 Pi Camera Module 3", display);

        if (cv::waitKey(1) == 27) break;
    }

    // ===== cleanup =====
    TfLiteInterpreterDelete(interpreter);
    TfLiteModelDelete(model);

    return 0;
}
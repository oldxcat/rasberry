#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

extern "C" {
#include "tensorflow/lite/c/c_api.h"
}

// 🔥 설정값
#define CONF_THRESH 0.2
#define IOU_THRESH 0.8

// 클래스 (bird 하나)
const char* class_names[] = {"bird"};

cv::Mat shared_frame;
std::mutex frame_mutex;
bool running = true;

// ===== 카메라 스레드 =====
void capture_thread_func(cv::VideoCapture& cap) {
    cv::Mat frame;
    while (running) {
        cap >> frame;
        if (frame.empty()) continue;

        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(shared_frame);
    }
}

int main() {
    printf("YOLOv8 + Class Label Start\n");

    // ===== TFLite =====
    TfLiteModel* model = TfLiteModelCreateFromFile("best_int8.tflite");
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, 4);

    TfLiteInterpreter* interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);

    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(interpreter, 0);
    const TfLiteTensor* output = TfLiteInterpreterGetOutputTensor(interpreter, 0);

    // ===== GStreamer (안정 버전) =====
    std::string pipeline =
        "libcamerasrc ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! "
        "appsink";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        printf("카메라 열기 실패\n");
        return -1;
    }

    std::thread cap_thread(capture_thread_func, std::ref(cap));

    double fps = 0;

    while (1) {
        double start = (double)cv::getTickCount();

        cv::Mat frame;

        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (shared_frame.empty()) continue;
            shared_frame.copyTo(frame);
        }

        // ===== 전처리 =====
        cv::Mat resized, rgb;
        cv::resize(frame, resized, cv::Size(640, 640));
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

        float* input_ptr = (float*)TfLiteTensorData(input);
        memcpy(input_ptr, rgb.data, 640 * 640 * 3 * sizeof(float));

        // ===== 추론 =====
        TfLiteInterpreterInvoke(interpreter);

        float* out = (float*)TfLiteTensorData(output);
        const int num_elements = 8400;

        std::vector<cv::Rect> bboxes;
        std::vector<float> scores;
        std::vector<int> class_ids;

        for (int i = 0; i < num_elements; i++) {
            float obj = out[4 * num_elements + i];
            float cls = out[5 * num_elements + i];  // 클래스 1개

            float conf = obj * cls;
            if (conf < CONF_THRESH) continue;

            float x_c = out[0 * num_elements + i];
            float y_c = out[1 * num_elements + i];
            float w   = out[2 * num_elements + i];
            float h   = out[3 * num_elements + i];

            

            int x1 = (int)((x_c - w / 2.0f) * frame.cols);
            int y1 = (int)((y_c - h / 2.0f) * frame.rows);
            int w_px = (int)(w * frame.cols);
            int h_px = (int)(h * frame.rows);

            bboxes.push_back(cv::Rect(x1, y1, w_px, h_px));
            scores.push_back(conf);
            class_ids.push_back(0); // bird
        }

        // ===== NMS =====
        std::vector<int> indices;
        if (!bboxes.empty()) {
            cv::dnn::NMSBoxes(bboxes, scores, CONF_THRESH, IOU_THRESH, indices);
        }

        // ===== 출력 =====
        for (int idx : indices) {
            cv::rectangle(frame, bboxes[idx], cv::Scalar(0, 255, 0), 2);

            // 🔥 클래스 + confidence 표시
            char label[50];
            sprintf(label, "%s %.2f", class_names[class_ids[idx]], scores[idx]);

            cv::putText(frame, label,
                        cv::Point(bboxes[idx].x, bboxes[idx].y - 5),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.5,
                        cv::Scalar(0, 255, 0),
                        2);
        }

        // ===== FPS =====
        double end = (double)cv::getTickCount();
        fps = 1.0 / ((end - start) / cv::getTickFrequency());

        char fps_text[30];
        sprintf(fps_text, "FPS: %.1f", fps);
        cv::putText(frame, fps_text, cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0, 255, 255), 2);

        cv::imshow("YOLOv8 Detection", frame);

        if (cv::waitKey(1) == 27) break;
    }

    running = false;
    cap_thread.join();

    cap.release();

    TfLiteInterpreterDelete(interpreter);
    TfLiteModelDelete(model);

    return 0;
}
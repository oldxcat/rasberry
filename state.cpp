#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std;

enum class DroneState {
    IDLE,
    TAKEOFF,
    CRUISE,
    COMPLETE,
    RETURN,
    LAND
};

struct EventData {
    bool target_detected = false;
    bool obstacle_detected = false;
};

EventData sharedData;
mutex dataMutex;
atomic<bool> running(true);

void sensorThread() {
    while (running) {
        {
            lock_guard<mutex> lock(dataMutex);
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

class DroneController {
private:
    DroneState state = DroneState::IDLE;

public:
    void run() {
        while (running) {
            update();
            checkEvents();
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

private:
    void setState(DroneState newState) {
        cout << "[STATE] " << stateToString(state)
             << " -> " << stateToString(newState) << endl;
        state = newState;
    }

    string stateToString(DroneState s) {
        switch (s) {
            case DroneState::IDLE: return "IDLE";
            case DroneState::TAKEOFF: return "TAKEOFF";
            case DroneState::CRUISE: return "CRUISE";
            case DroneState::COMPLETE: return "COMPLETE";
            case DroneState::RETURN: return "RETURN";
            case DroneState::LAND: return "LAND";
            default: return "UNKNOWN";
        }
    }

    void update() {
        switch (state) {
            case DroneState::IDLE:
                cout << "대기 중..." << endl;
                setState(DroneState::TAKEOFF);
                break;

            case DroneState::TAKEOFF:
                cout << "이륙 중..." << endl;
                setState(DroneState::CRUISE);
                break;

            case DroneState::CRUISE:
                cout << "이동 중..." << endl;
                break;

            case DroneState::COMPLETE:
                cout << "임무 완료" << endl;
                setState(DroneState::RETURN);
                break;

            case DroneState::RETURN:
                cout << "복귀 중..." << endl;
                setState(DroneState::LAND);
                break;

            case DroneState::LAND:
                cout << "착륙 완료" << endl;
                running = false;
                break;
        }
    }

    void checkEvents() {
        lock_guard<mutex> lock(dataMutex);
    }
};

int main() {
    thread sensor(sensorThread);

    DroneController drone;
    drone.run();

    sensor.join();
    return 0;
}
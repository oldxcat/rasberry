# DroneServer MQTT 클라이언트 구현 가이드

## 시스템 개요

현재 DroneServer는 HTTP 요청을 MQTT 메시지로 변환하는 브릿지 역할을 합니다. 드론 클라이언트는 MQTT를 통해 명령어를 수신하고 처리할 수 있습니다.

## MQTT 브로커 정보

- **호스트**: localhost (또는 127.0.0.1)
- **포트**: 1883
- **브로커**: Mosquitto
- **프로토콜**: MQTT 3.1.1

## 메시지 형식

### 수신할 메시지 토픽
```
drone/commands
```

### 메시지 페이로드 형식
```
{droneID},{orderID},{command}
```

**예시:**
```
1,1,startdelivery
```

**필드 설명:**
- `droneID`: 드론 ID (현재 하드코딩: 1)
- `orderID`: 주문 ID (현재 하드코딩: 1)
- `command`: 실행 명령어 (현재: "startdelivery")

## Java 서버 엔드포인트

### HTTP 요청
```
GET http://localhost:8080/requestfood/{foodId}
```

**응답:**
```
Message sent: {droneID},{orderID},startdelivery
```

**예시:**
```bash
curl http://localhost:8080/requestfood/1
# 응답: Message sent: 1,1,startdelivery
```

## 드론 클라이언트 구현 요구사항

### 1. MQTT 클라이언트 라이브러리
- **Python**: `paho-mqtt`
- **C++**: `paho-mqttpp3`
- **JavaScript/Node.js**: `mqtt`
- **C#/.NET**: `MQTTnet`
- **Java**: `paho.mqtt.java`

### 2. 필수 기능

#### a) MQTT 연결
```
- 호스트에 연결
- 포트 1883 사용
- keep-alive 설정
- clean session 활성화
```

#### b) 토픽 구독
```
- "drone/commands" 토픽 구독
- QoS 1 레벨 권장
```

#### c) 메시지 수신 및 파싱
```
- 메시지 페이로드 수신
- ','로 구분하여 파싱
- droneID, orderID, command 추출
```

#### d) 명령어 처리
```
command = "startdelivery" 연산 처리 로직
- 배송 시작
- 드론 이동
- 목표 지점 도달
- 배송 완료
```

### 3. Python 예시 구현

```python
import paho.mqtt.client as mqtt
import json

class DroneClient:
    def __init__(self, drone_id, broker_host="localhost", broker_port=1883):
        self.drone_id = drone_id
        self.client = mqtt.Client(client_id=f"drone_{drone_id}")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.broker_host = broker_host
        self.broker_port = broker_port

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"Drone {self.drone_id}: Connected to broker")
            client.subscribe("drone/commands", qos=1)
        else:
            print(f"Drone {self.drone_id}: Connection failed with code {rc}")

    def on_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode()
            parts = payload.split(',')
            
            if len(parts) == 3:
                drone_id, order_id, command = parts
                
                # 자신의 드론 ID 확인
                if str(drone_id) == str(self.drone_id):
                    print(f"Drone {self.drone_id}: Received command")
                    print(f"  Order ID: {order_id}")
                    print(f"  Command: {command}")
                    
                    # 명령어 처리
                    self.handle_command(order_id, command)
        except Exception as e:
            print(f"Error parsing message: {e}")

    def handle_command(self, order_id, command):
        if command == "startdelivery":
            print(f"Drone {self.drone_id}: Starting delivery for order {order_id}")
            # 배송 로직 구현
            print(f"Drone {self.drone_id}: Delivery started")
        else:
            print(f"Drone {self.drone_id}: Unknown command: {command}")

    def start(self):
        self.client.connect(self.broker_host, self.broker_port, keepalive=60)
        self.client.loop_forever()

# 사용 예시
if __name__ == "__main__":
    drone = DroneClient(drone_id=1)
    drone.start()
```

### 4. C++ 예시 구현

```cpp
#include <mqtt/client.h>
#include <iostream>
#include <string>
#include <vector>

class DroneClient {
private:
    std::string drone_id;
    mqtt::client* client;
    
public:
    DroneClient(const std::string& id) : drone_id(id) {}
    
    void handleMessage(const std::string& payload) {
        // 메시지 파싱: droneID,orderID,command
        size_t pos1 = payload.find(',');
        size_t pos2 = payload.find(',', pos1 + 1);
        
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string drone_id_received = payload.substr(0, pos1);
            std::string order_id = payload.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string command = payload.substr(pos2 + 1);
            
            // 자신의 드론 ID 확인
            if (drone_id_received == drone_id) {
                std::cout << "Drone " << drone_id << ": Received command" << std::endl;
                std::cout << "  Order ID: " << order_id << std::endl;
                std::cout << "  Command: " << command << std::endl;
                
                handleCommand(order_id, command);
            }
        }
    }
    
    void handleCommand(const std::string& order_id, const std::string& command) {
        if (command == "startdelivery") {
            std::cout << "Drone " << drone_id << ": Starting delivery for order " << order_id << std::endl;
            // 배송 로직 구현
        }
    }
};
```

## 현재 상태

### 완성된 것
- ✅ HTTP 엔드포인트: `/requestfood/{foodId}`
- ✅ MQTT 클라이언트 (Java)
- ✅ MQTT 브로커 (Mosquitto)
- ✅ 메시지 형식 정의

### 구현해야 할 것
- ⬜ 드론 클라이언트 (MQTT 구독)
- ⬜ 명령어 처리 로직
- ⬜ 드론 소프트웨어 통합

## 테스트 방법

### 1. 개별 테스트

**Java 서버 실행:**
```bash
java -jar target/drone-server-0.0.1-SNAPSHOT.jar
```

**MQTT 브로커 실행:**
```bash
mosquitto -p 1883
```

**드론 클라이언트 테스트:**
```bash
# Python
python mqtt_test.py

# 또는 Java 테스트 프로그램 실행
```

### 2. 통합 테스트

**Step 1:** MQTT 브로커 시작
```bash
mosquitto -p 1883
```

**Step 2:** Java 서버 시작
```bash
java -jar target/drone-server-0.0.1-SNAPSHOT.jar
```

**Step 3:** 드론 클라이언트 시작
```bash
python drone_client.py
```

**Step 4:** HTTP 요청 전송
```bash
curl http://localhost:8080/requestfood/1
```

**Expected Output (드론 콘솔):**
```
Drone 1: Received command
  Order ID: 1
  Command: startdelivery
Drone 1: Starting delivery for order 1
```

## 주요 참고사항

1. **드론 ID**: 현재 하드코딩되어 있습니다 (`AssignedDroneIdGenerator.java`)
   - 추후 동적으로 변경 가능하도록 설계됨

2. **주문 ID**: 현재 하드코딩되어 있습니다 (`OrderIdGenerator.java`)
   - 추후 주문 시스템과 통합 예정

3. **메시지 QoS**: 현재 QoS 1로 설정
   - 변경 필요 시 `MqttService.java` 수정

4. **토픽**: `drone/commands`
   - 변경 필요 시 `application.properties` 수정

## 파일 위치

- **Java 서버**: `/src/main/java/com/example/droneserver/`
- **MQTT 설정**: `application.properties`
- **C++ 서버**: `/mqtt-server-cpp/`
- **Python 테스트**: `mqtt_test.py`

## 다음 단계

1. 드론 클라이언트 MQTT 라이브러리 선택 (Python, C++, Java 등)
2. 드론 클라이언트 구현 및 테스트
3. 명령어 처리 로직 구현
4. 배송 시그널 및 상태 반환 (옵션)
5. 드론 소프트웨어 통합

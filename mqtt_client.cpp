#include <mqtt/client.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <fstream>
#include <unordered_map>

std::unordered_map<std::string, std::string> loadConfig(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        return config;
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            config[key] = value;
        }
    }
    return config;
}

class MqttCommandReceiver : public mqtt::callback {
private:
    std::string drone_id;
    std::string broker_host;
    int broker_port;
    mqtt::client* client;
    std::atomic<bool> running;
    std::function<void(int, int, const std::string&)> command_handler;

public:
    MqttCommandReceiver(const std::string& config_file, std::function<void(int, int, const std::string&)> handler)
        : client(nullptr), running(true), command_handler(handler) {
        auto config = loadConfig(config_file);
        drone_id = config["drone_id"];
        broker_host = config["broker_host"];
        broker_port = std::stoi(config["broker_port"]);
    }

    ~MqttCommandReceiver() {
        stop();
        if (client) {
            delete client;
        }
    }

    void start() {
        try {
            client = new mqtt::client(broker_host, broker_port, "drone_" + drone_id);
            client->set_callback(*this);

            mqtt::connect_options conn_opts;
            conn_opts.set_keep_alive_interval(60);
            conn_opts.set_clean_session(true);

            std::cout << "Connecting to MQTT broker at " << broker_host << ":" << broker_port << std::endl;
            client->connect(conn_opts);

            client->subscribe("drone/commands", 1);

            std::cout << "MQTT command receiver started for drone " << drone_id << std::endl;

            // Start loop in a separate thread
            std::thread loop_thread([this]() {
                while (running) {
                    client->yield(100); // Yield to allow processing
                }
            });
            loop_thread.detach();

        } catch (const mqtt::exception& exc) {
            std::cerr << "MQTT error: " << exc.what() << std::endl;
        }
    }

    void stop() {
        running = false;
        if (client && client->is_connected()) {
            client->disconnect();
        }
    }

    // MQTT callback
    void connected(const std::string& cause) override {
        std::cout << "Connected to MQTT broker" << std::endl;
    }

    void connection_lost(const std::string& cause) override {
        std::cout << "Connection lost: " << cause << std::endl;
        if (running) {
            // Reconnect logic can be added here
        }
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string payload = msg->get_payload_str();
        std::cout << "Message received: " << payload << std::endl;

        // Parse the message
        parse_and_handle_command(payload);
    }

private:
    void parse_and_handle_command(const std::string& payload) {
        // Expected format: droneID,orderID,command
        size_t pos1 = payload.find(',');
        if (pos1 == std::string::npos) return;

        size_t pos2 = payload.find(',', pos1 + 1);
        if (pos2 == std::string::npos) return;

        try {
            int received_drone_id = std::stoi(payload.substr(0, pos1));
            int order_id = std::stoi(payload.substr(pos1 + 1, pos2 - pos1 - 1));
            std::string command = payload.substr(pos2 + 1);

            // Check if this message is for this drone
            if (received_drone_id == std::stoi(drone_id)) {
                std::cout << "Command for this drone: order_id=" << order_id << ", command=" << command << std::endl;
                if (command_handler) {
                    command_handler(received_drone_id, order_id, command);
                }
            } else {
                std::cout << "Message not for this drone (id=" << received_drone_id << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing message: " << e.what() << std::endl;
        }
    }
};

// Command handler function (to be defined elsewhere, e.g., in DroneController)
void handle_command(int drone_id, int order_id, const std::string& command) {
    if (command == "startdelivery") {
        std::cout << "Starting delivery for order " << order_id << " on drone " << drone_id << std::endl;
        // Here you can integrate with DroneController to change state
        // For example: drone_controller.start_delivery(order_id);
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
}

int main() {
    // Example usage
    MqttCommandReceiver receiver("config.properties", handle_command);
    receiver.start();

    // Keep the main thread running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
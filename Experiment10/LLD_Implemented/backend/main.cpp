#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static const std::string RESTAURANTS_JSON = R"([
  {"id":"r1","name":"The Spice","rating":4.3,"eta_min":20,"eta_max":30},
  {"id":"r2","name":"Green Bowl","rating":4.6,"eta_min":15,"eta_max":25}
])";

static const std::string MENU_R1 = R"({"items":[{"id":"m1","name":"Paneer Tikka","price":24900},{"id":"m2","name":"Naan","price":3000}]})";
static const std::string MENU_R2 = R"({"items":[{"id":"m3","name":"Quinoa Salad","price":19900},{"id":"m4","name":"Lemonade","price":5000}]})";

std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string make_response(const std::string &body, const std::string &status = "200 OK", const std::string &contentType = "application/json") {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Connection: close\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
}

std::string generate_id() {
    static uint64_t counter = 0;
    ++counter;
    std::ostringstream ss;
    ss << "o_" << std::time(nullptr) << "_" << counter;
    return ss.str();
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1 || server_fd == INVALID_SOCKET) {
#ifdef _WIN32
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
#else
        perror("socket");
#endif
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
#ifdef _WIN32
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
#else
        perror("bind");
        close(server_fd);
#endif
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
#ifdef _WIN32
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
#else
        perror("listen");
        close(server_fd);
#endif
        return 1;
    }

    std::cout << "Backend listening on http://0.0.0.0:8080" << std::endl;

    std::unordered_map<std::string, std::string> idempotency_map;

    while (true) {
#ifdef _WIN32
        SOCKET client = accept(server_fd, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }
#else
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) {
            perror("accept");
            continue;
        }
#endif

        std::string req;
        char buffer[4096];
#ifdef _WIN32
        int n = recv(client, buffer, sizeof(buffer), 0);
#else
        ssize_t n = recv(client, buffer, sizeof(buffer), 0);
#endif
        if (n <= 0) {
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            continue;
        }
        req.append(buffer, buffer + n);

        // Read until end of headers
        while (req.find("\r\n\r\n") == std::string::npos) {
#ifdef _WIN32
            n = recv(client, buffer, sizeof(buffer), 0);
#else
            n = recv(client, buffer, sizeof(buffer), 0);
#endif
            if (n <= 0) break;
            req.append(buffer, buffer + n);
        }

        // Parse request line
        std::istringstream rs(req);
        std::string request_line;
        std::getline(rs, request_line);
        if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();

        std::string method, path, httpver;
        std::istringstream rl(request_line);
        rl >> method >> path >> httpver;

        // Parse headers
        std::map<std::string, std::string> headers;
        std::string line;
        while (std::getline(rs, line) && line != "\r" && !line.empty()) {
            if (line.back() == '\r') line.pop_back();
            auto pos = line.find(":");
            if (pos != std::string::npos) {
                std::string k = line.substr(0, pos);
                std::string v = line.substr(pos + 1);
                // trim
                while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
                std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                headers[k] = v;
            }
        }

        std::string body;
        auto it = headers.find("content-length");
        if (it != headers.end()) {
            int content_length = std::stoi(it->second);
            size_t body_start = req.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                body_start += 4;
                if (req.size() >= body_start + content_length) {
                    body = req.substr(body_start, content_length);
                } else {
                    // read remaining
                    int remaining = content_length - (req.size() - body_start);
                    while (remaining > 0) {
#ifdef _WIN32
                        n = recv(client, buffer, sizeof(buffer), 0);
                        if (n <= 0) break;
                        body.append(buffer, buffer + n);
                        remaining -= n;
#else
                        n = recv(client, buffer, sizeof(buffer), 0);
                        if (n <= 0) break;
                        body.append(buffer, buffer + n);
                        remaining -= n;
#endif
                    }
                }
            }
        }

        std::string resp;

        if (method == "GET" && path == "/v1/restaurants") {
            resp = make_response(RESTAURANTS_JSON);
        } else if (method == "GET" && path.rfind("/v1/restaurants/", 0) == 0 && path.find("/menu", 12) != std::string::npos) {
            // path: /v1/restaurants/{id}/menu
            auto parts = path.substr(13); // after /v1/restaurants/
            std::string id = parts.substr(0, parts.find('/'));
            if (id == "r1") resp = make_response(MENU_R1);
            else if (id == "r2") resp = make_response(MENU_R2);
            else resp = make_response("{\"items\":[]}");
        } else if (method == "POST" && path == "/v1/orders") {
            std::string idemKey;
            auto it2 = headers.find("idempotency-key");
            if (it2 != headers.end()) idemKey = it2->second;

            if (!idemKey.empty()) {
                auto found = idempotency_map.find(idemKey);
                if (found != idempotency_map.end()) {
                    // return previous response
                    std::ostringstream ob;
                    ob << "{\"order_id\":\"" << found->second << "\",\"status\":\"CONFIRMED\"}";
                    resp = make_response(ob.str(), "200 OK");
                } else {
                    std::string oid = generate_id();
                    idempotency_map[idemKey] = oid;
                    std::ostringstream ob;
                    ob << "{\"order_id\":\"" << oid << "\",\"status\":\"CONFIRMED\",\"created_at\":\"" << now_iso() << "\"}";
                    resp = make_response(ob.str(), "201 Created");
                }
            } else {
                // no idempotency key provided
                std::string oid = generate_id();
                std::ostringstream ob;
                ob << "{\"order_id\":\"" << oid << "\",\"status\":\"CONFIRMED\"}";
                resp = make_response(ob.str(), "201 Created");
            }
        } else if (method == "OPTIONS") {
            // CORS preflight
            std::ostringstream ss;
            ss << "HTTP/1.1 200 OK\r\n";
            ss << "Access-Control-Allow-Origin: *\r\n";
            ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
            ss << "Access-Control-Allow-Headers: Authorization, Idempotency-Key, Content-Type\r\n";
            ss << "Content-Length: 0\r\n";
            ss << "\r\n";
            resp = ss.str();
        } else {
            resp = make_response("{\"error\":\"not found\"}", "404 Not Found");
        }

#ifdef _WIN32
        send(client, resp.c_str(), (int)resp.size(), 0);
        closesocket(client);
#else
        send(client, resp.c_str(), resp.size(), 0);
        close(client);
#endif
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

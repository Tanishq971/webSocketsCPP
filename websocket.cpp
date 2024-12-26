#include<boost/asio.hpp> 
#include<boost/beast/core.hpp>
#include<boost/beast/websocket.hpp>
#include<nlohmann/json.hpp>
#include<iostream>
#include<sstream>
#include<string>
#include<thread>
#include<mutex>
#include<vector>
#include<memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients;
std::mutex clients_mutex;

void broadcast_message(const std::string& message) {
  std::lock_guard<std::mutex> lock(clients_mutex);
  for (auto it = clients.begin(); it != clients.end();) {
    try {
      if(*it) {
        auto& ws = **it;
        ws.text(ws.got_text());
        ws.write(asio::buffer(message));
        ++it;
      } else {
        it = clients.erase(it);
      }
    } catch(const std::exception& e) {
        std::cerr << "Error: " << std::endl;
        it = clients.erase(it);
    }
  }
}

void websocket_session(tcp::socket socket) {
  auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
  try {
    ws->accept();
    std::cout << "Connection Established" << std::endl;
    {
      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.push_back(ws);
    }

    for(;;) {
      beast::flat_buffer buffer;
      ws->read(buffer);
      std::string recieved_msg = beast::buffers_to_string(buffer.data());
      std::cout << "Recieved: " << recieved_msg << std::endl;

      broadcast_message(recieved_msg);
    }
  } catch(const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      std::lock_guard<std::mutex> lock(clients_mutex);
      auto it = std::find(clients.begin(), clients.end(), ws);
      if(it != clients.end()) {
        clients.erase(it);
      }
  }
}

int main() {
  try {
    asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8090));
    std::cout << "Websocket running on port 8090" << std::endl;

    while (true) {
      tcp::socket socket(io_context);
      acceptor.accept(socket);
      
      std::thread(websocket_session, std::move(socket)).detach();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error + " << e.what() << std::endl;
  }
  return 0;
}
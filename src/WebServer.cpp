#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Game.hpp"
#include "CommandProcessor.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class http_connection : public std::enable_shared_from_this<http_connection> {
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    Game& game_;
    CommandProcessor& processor_;
    std::string client_address_;

public:
    http_connection(tcp::socket&& socket, Game& game, CommandProcessor& processor)
        : stream_(std::move(socket))
        , game_(game)
        , processor_(processor) {
            client_address_ = stream_.socket().remote_endpoint().address().to_string() + 
                            ":" + std::to_string(stream_.socket().remote_endpoint().port());
            std::cout << "New connection from " << client_address_ << std::endl;
        }

    void start() {
        std::cout << "Starting connection handling for " << client_address_ << std::endl;
        read_request();
    }

private:
    void read_request() {
        auto self = shared_from_this();

        http::async_read(
            stream_,
            buffer_,
            req_,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                if(ec == http::error::end_of_stream) {
                    std::cout << "Client closed connection: " << self->client_address_ << std::endl;
                    return self->do_close();
                }
                if(ec) {
                    std::cerr << "Error reading request from " << self->client_address_ 
                             << ": " << ec.message() << std::endl;
                    return;
                }
                self->handle_request();
            });
    }

    void do_close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        if(ec) {
            std::cerr << "Error closing connection for " << client_address_ 
                     << ": " << ec.message() << std::endl;
        }
    }

    bool ends_with(const std::string& str, const std::string& suffix) {
        if (str.length() < suffix.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    std::string get_mime_type(const std::string& path) {
        if (ends_with(path, ".html")) return "text/html";
        if (ends_with(path, ".css")) return "text/css";
        if (ends_with(path, ".js")) return "application/javascript";
        return "application/octet-stream";
    }

    void send_file(const std::string& path) {
        std::cout << "\nSending file request from " << client_address_ << std::endl;
        std::cout << "Trying to send file: " << path << std::endl;
        
        try {
            std::filesystem::path abs_path = std::filesystem::absolute(path);
            std::cout << "Absolute path: " << abs_path.string() << std::endl;
            
            if (!std::filesystem::exists(abs_path)) {
                std::cerr << "File does not exist: " << abs_path.string() << std::endl;
                return send_bad_response(http::status::not_found, "File not found");
            }
            
            if (!std::filesystem::is_regular_file(abs_path)) {
                std::cerr << "Not a regular file: " << abs_path.string() << std::endl;
                return send_bad_response(http::status::not_found, "Not a regular file");
            }
            
            auto file_size = std::filesystem::file_size(abs_path);
            std::cout << "File size: " << file_size << " bytes" << std::endl;
            
            std::ifstream file(abs_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to open file: " << abs_path.string() << std::endl;
                return send_bad_response(http::status::internal_server_error, "Failed to open file");
            }
            
            auto res = std::make_shared<http::response<http::string_body>>();
            res->version(req_.version());
            res->result(http::status::ok);
            res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res->set(http::field::content_type, get_mime_type(path));
            
            res->set(http::field::cache_control, "no-store, no-cache, must-revalidate, max-age=0");
            res->set(http::field::pragma, "no-cache");
            res->set(http::field::expires, "0");
            
            res->set(http::field::access_control_allow_origin, "*");
            res->set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
            res->set(http::field::access_control_allow_headers, "Content-Type");
            
            bool keep_alive = req_.keep_alive();
            res->keep_alive(keep_alive);
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            if (file.fail()) {
                std::cerr << "Failed to read file: " << abs_path.string() << std::endl;
                return send_bad_response(http::status::internal_server_error, "Failed to read file");
            }
            
            file.close();
            
            res->body() = std::move(content);
            res->prepare_payload();
            
            std::cout << "Prepared response:" << std::endl;
            std::cout << "Status: " << res->result_int() << std::endl;
            std::cout << "Headers:" << std::endl;
            for(auto const& field : res->base()) {
                std::cout << field.name_string() << ": " << field.value() << std::endl;
            }
            std::cout << "Body size: " << res->body().size() << " bytes" << std::endl;
            
            auto self = shared_from_this();
            http::async_write(
                stream_,
                *res,
                [self, res, keep_alive](beast::error_code ec, std::size_t bytes_transferred) {
                    self->on_write(ec, bytes_transferred, !keep_alive);
                });
            
        } catch (const std::exception& e) {
            std::cerr << "Exception while sending file: " << e.what() << std::endl;
            return send_bad_response(http::status::internal_server_error, e.what());
        }
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred, bool close) {
        if(ec) {
            std::cerr << "Error writing response to " << client_address_ 
                     << ": " << ec.message() << std::endl;
            return;
        }
        
        std::cout << "Successfully sent " << bytes_transferred << " bytes" << std::endl;
        
        if(close) {
            return do_close();
        }
        
        buffer_.consume(buffer_.size());
        read_request();
    }

    void send(http::response<http::string_body>&& res) {
        auto self = shared_from_this();
        
        http::async_write(
            stream_,
            res,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                if(ec) {
                    std::cerr << "Error writing response to " << self->client_address_ 
                             << ": " << ec.message() << std::endl;
                    return;
                }
                
                if(self->req_.need_eof()) {
                    self->do_close();
                    return;
                }
                
                self->buffer_.consume(self->buffer_.size());
                self->read_request();
            });
    }

    void handle_request() {
        std::cout << "\n=== New Request ===" << std::endl;
        std::cout << "Method: " << req_.method_string() << std::endl;
        std::cout << "Target: " << req_.target() << std::endl;
        std::cout << "HTTP version: " << req_.version() << std::endl;
        std::cout << "Headers:" << std::endl;
        for(auto const& field : req_.base()) {
            std::cout << field.name_string() << ": " << field.value() << std::endl;
        }
        std::cout << "==================" << std::endl;
        
        if (req_.method() == http::verb::options) {
            send_cors_headers(http::status::no_content);
            return;
        }

        if (req_.method() == http::verb::get) {
            std::string target = std::string(req_.target());
            std::cout << "GET request for: " << target << std::endl;
            
            std::filesystem::path base_path = std::filesystem::absolute("../web");
            std::cout << "Base web directory: " << base_path.string() << std::endl;
            
            if (target == "/" || target == "/index.html") {
                send_file((base_path / "index.html").string());
            } else if (target == "/styles.css") {
                send_file((base_path / "styles.css").string());
            } else if (target == "/script.js") {
                send_file((base_path / "script.js").string());
            } else if (target == "/ships") {
                boost::json::object response;
                boost::json::array ships_array;
                
                const auto& myShips = game_.getMyShips();
                for(const auto& ship : myShips) {
                    boost::json::object ship_obj;
                    ship_obj["x"] = ship.getX();
                    ship_obj["y"] = ship.getY();
                    ship_obj["size"] = ship.getSize();
                    ship_obj["horizontal"] = ship.isHorizontal();
                    ships_array.push_back(ship_obj);
                }
                
                response["ships"] = ships_array;
                
                auto res = std::make_shared<http::response<http::string_body>>();
                res->version(req_.version());
                res->result(http::status::ok);
                res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res->set(http::field::content_type, "application/json");
                res->set(http::field::access_control_allow_origin, "*");
                res->set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
                res->set(http::field::access_control_allow_headers, "Content-Type");
                res->keep_alive(req_.keep_alive());
                
                res->body() = boost::json::serialize(response);
                res->prepare_payload();

                auto self = shared_from_this();
                http::async_write(
                    stream_,
                    *res,
                    [self, res, keep_alive = req_.keep_alive()](beast::error_code ec, std::size_t bytes_transferred) {
                        if(ec) {
                            std::cerr << "Error sending response: " << ec.message() << std::endl;
                        }
                        if (!keep_alive) {
                            self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                        }
                    });
            } else if (target == "/game-state") {
                auto res = std::make_shared<http::response<http::string_body>>();
                res->version(req_.version());
                res->result(http::status::ok);
                res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res->set(http::field::content_type, "application/json");
                res->set(http::field::access_control_allow_origin, "*");
                res->set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
                res->set(http::field::access_control_allow_headers, "Content-Type");
                res->keep_alive(req_.keep_alive());
                
                handle_get_game_state(req_, *res, game_);
                
                auto self = shared_from_this();
                http::async_write(
                    stream_,
                    *res,
                    [self, res, keep_alive = req_.keep_alive()](beast::error_code ec, std::size_t bytes_transferred) {
                        if(ec) {
                            std::cerr << "Error sending response: " << ec.message() << std::endl;
                        }
                        if (!keep_alive) {
                            self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                        }
                    });
            } else if (target == "/shots") {
                handle_shots();
            } else {
                std::cout << "File not found: " << target << std::endl;
                send_bad_response(http::status::not_found, "File not found");
            }
            return;
        }

        if (req_.method() != http::verb::post) {
            send_bad_response(http::status::bad_request, "Invalid method");
            return;
        }

        try {
            auto json = boost::json::parse(req_.body());
            auto& obj = json.as_object();
            std::string command = obj["command"].as_string().c_str();
            
            std::cout << "Processing command: " << command << std::endl;
            std::string response = processor_.processCommand(command);
            std::cout << "Command response: " << response << std::endl;

            boost::json::object json_response;
            json_response["response"] = response;
            
            auto res = std::make_shared<http::response<http::string_body>>();
            res->version(req_.version());
            res->result(http::status::ok);
            res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res->set(http::field::content_type, "application/json");
            res->set(http::field::access_control_allow_origin, "*");
            res->set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
            res->set(http::field::access_control_allow_headers, "Content-Type");
            res->keep_alive(req_.keep_alive());
            
            res->body() = boost::json::serialize(json_response);
            res->prepare_payload();

            auto self = shared_from_this();
            http::async_write(
                stream_,
                *res,
                [self, res, keep_alive = req_.keep_alive()](beast::error_code ec, std::size_t bytes_transferred) {
                    if(ec) {
                        std::cerr << "Error sending response: " << ec.message() << std::endl;
                    }
                    if (!keep_alive) {
                        self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                    }
                });
        }
        catch(const std::exception& e) {
            std::cerr << "Error processing request: " << e.what() << std::endl;
            send_bad_response(http::status::bad_request, e.what());
        }
    }

    void handle_get_game_state(const http::request<http::string_body>& req,
                          http::response<http::string_body>& res,
                          Game& game) {
        boost::json::object response;
        
        auto& myBoard = game.getPlayerBoard();
        auto& enemyBoard = game.getEnemyBoard();
        
        boost::json::array myBoardJson;
        boost::json::array enemyBoardJson;

        for (size_t i = 0; i < myBoard.size(); ++i) {
            boost::json::array row1, row2;
            for (size_t j = 0; j < myBoard[i].size(); ++j) {
                row1.push_back(static_cast<int>(myBoard[i][j]));
                row2.push_back(static_cast<int>(enemyBoard[i][j]));
            }
            myBoardJson.push_back(row1);
            enemyBoardJson.push_back(row2);
        }
        
        response["myBoard"] = myBoardJson;
        response["enemyBoard"] = enemyBoardJson;
        
        res.body() = boost::json::serialize(response);
        res.prepare_payload();
    }

    void handle_shots() {
        boost::json::object response;
        
        // выстрелы
        boost::json::array playerShots = boost::json::array();
        boost::json::array enemyShots = boost::json::array();
        
        // выстрелы с игрока
        for (size_t y = 0; y < game_.getHeight(); ++y) {
            for (size_t x = 0; x < game_.getWidth(); ++x) {
                CellState state = game_.getPlayerBoard()[y][x];
                if (state == CellState::HIT || state == CellState::MISS) {
                    boost::json::object shot;
                    shot["x"] = x;
                    shot["y"] = y;
                    shot["result"] = (state == CellState::HIT) ? "hit" : "miss";
                    enemyShots.push_back(shot);
                }
            }
        }
        
        // выстрелы с противника
        for (size_t y = 0; y < game_.getHeight(); ++y) {
            for (size_t x = 0; x < game_.getWidth(); ++x) {
                CellState state = game_.getEnemyBoard()[y][x];
                if (state == CellState::HIT || state == CellState::MISS) {
                    boost::json::object shot;
                    shot["x"] = x;
                    shot["y"] = y;
                    shot["result"] = (state == CellState::HIT) ? "hit" : "miss";
                    playerShots.push_back(shot);
                }
            }
        }
        
        response["playerShots"] = playerShots;
        response["enemyShots"] = enemyShots;
        
        auto res = std::make_shared<http::response<http::string_body>>();
        res->version(req_.version());
        res->result(http::status::ok);
        res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res->set(http::field::content_type, "application/json");
        res->set(http::field::access_control_allow_origin, "*");
        res->set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
        res->set(http::field::access_control_allow_headers, "Content-Type");
        res->keep_alive(req_.keep_alive());
        
        res->body() = boost::json::serialize(response);
        res->prepare_payload();

        auto self = shared_from_this();
        http::async_write(
            stream_,
            *res,
            [self, res, keep_alive = req_.keep_alive()](beast::error_code ec, std::size_t bytes_transferred) {
                if(ec) {
                    std::cerr << "Error sending response: " << ec.message() << std::endl;
                }
                if (!keep_alive) {
                    self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                }
            });
    }

    void send_response(const std::string& response) {
        auto self = shared_from_this();

        boost::json::object obj;
        obj["response"] = response;
        std::string body = boost::json::serialize(obj);

        http::response<http::string_body> res{http::status::ok, req_.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "POST, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type");

        bool keep_alive = req_.keep_alive();
        if (keep_alive) {
            res.set(http::field::connection, "keep-alive");
            res.keep_alive(true);
        } else {
            res.set(http::field::connection, "close");
        }

        res.body() = body;
        res.prepare_payload();

        http::async_write(
            stream_,
            res,
            [self, keep_alive](beast::error_code ec, std::size_t bytes_transferred) {
                if(ec) {
                    std::cerr << "Error sending response: " << ec.message() << std::endl;
                }
                if (!keep_alive) {
                    self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                }
            });
    }

    void send_cors_headers(http::status status) {
        auto self = shared_from_this();

        http::response<http::empty_body> res{status, req_.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "POST, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type");

        bool keep_alive = req_.keep_alive();
        if (keep_alive) {
            res.set(http::field::connection, "keep-alive");
            res.keep_alive(true);
        } else {
            res.set(http::field::connection, "close");
        }

        res.prepare_payload();

        http::async_write(
            stream_,
            res,
            [self, keep_alive](beast::error_code ec, std::size_t bytes_transferred) {
                if(ec) {
                    std::cerr << "Error sending CORS headers: " << ec.message() << std::endl;
                }
                if (!keep_alive) {
                    self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                }
            });
    }

    void send_bad_response(http::status status, const std::string& error) {
        auto res = std::make_shared<http::response<http::string_body>>();
        res->version(req_.version());
        res->result(status);
        res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res->set(http::field::content_type, "text/plain");
        res->set(http::field::cache_control, "no-store, no-cache, must-revalidate, max-age=0");
        res->set(http::field::pragma, "no-cache");
        res->keep_alive(req_.keep_alive());
        res->body() = error;
        res->prepare_payload();

        auto self = shared_from_this();
        http::async_write(
            stream_,
            *res,
            [self, res](beast::error_code ec, std::size_t bytes_transferred) {
                self->on_write(ec, bytes_transferred, true);
            });
    }
};

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    Game& game_;
    CommandProcessor& processor_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        Game& game,
        CommandProcessor& processor)
        : ioc_(ioc)
        , acceptor_(ioc)
        , game_(game)
        , processor_(processor)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if(ec) {
            std::cerr << "open: " << ec.message() << std::endl;
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec) {
            std::cerr << "set_option: " << ec.message() << std::endl;
            return;
        }

        acceptor_.bind(endpoint, ec);
        if(ec) {
            std::cerr << "bind: " << ec.message() << std::endl;
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if(ec) {
            std::cerr << "listen: " << ec.message() << std::endl;
            return;
        }
    }

    void run() {
        std::cout << "Starting to accept connections..." << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            ioc_,
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if(ec) {
            std::cerr << "accept: " << ec.message() << std::endl;
        }
        else {
            std::cout << "Accepted new connection" << std::endl;
            std::make_shared<http_connection>(
                std::move(socket),
                game_,
                processor_)->start();
        }

        do_accept();
    }
};

int main() {
    try {
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(8080);
        
        net::io_context ioc{1};
        
        Game game;
        CommandProcessor processor(game);
        
        std::make_shared<listener>(
            ioc,
            tcp::endpoint{address, port},
            game,
            processor)->run();
        
        std::cout << "Server running on http://localhost:" << port << std::endl;
        
        ioc.run();
        
        std::cout << "Server stopped" << std::endl;
        return EXIT_SUCCESS;
        
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

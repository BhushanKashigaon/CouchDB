

#include "nghttp2/asio_http2_server.h"
#include<iostream>

using namespace std;

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;

int main(int argc, char *argv[]) {
  boost::system::error_code ec;
  http2 server;

  server.handle("/", [](const request &req, const response &res) {
    res.write_head(200);
    res.end("OK\n");
  });

  if (server.listen_and_serve(ec, argv[1] , argv[2])) {
    std::cerr << "error: " << ec.message() << std::endl;
  }
  else{
     printf("Server started on IP : %s Port : %s", argv[1],  argv[2]);
}
}

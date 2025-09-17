#ifndef SEODISPARATE_COM_PMA_CXX_HTTP2_H_
#define SEODISPARATE_COM_PMA_CXX_HTTP2_H_

#include <bitset>
#include <cstdint>
#include <optional>
#include <string>

class Http2Server {
 public:
  Http2Server(uint16_t port, uint32_t scope_id = 0);
  ~Http2Server();

  void update();

  std::optional<std::string> get_error();

 private:
  // 0 - is invalid
  std::bitset<32> flags;
  std::optional<int> errno_cached;
  int socket_fd;
};

#endif

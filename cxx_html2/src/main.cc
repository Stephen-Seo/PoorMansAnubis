#include <iostream>

#include "http2.h"

int main(int argc, char **argv) {
  // TODO set up arg parsing to specify server port.
  Http2Server c(19191);

  if (auto err = c.get_error(); err.has_value()) {
    std::cerr << "Error in Http2Server:\n";
    std::cerr << err.value() << std::endl;
  }

  return 0;
}

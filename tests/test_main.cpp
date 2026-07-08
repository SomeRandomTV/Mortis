#include <cassert>
#include <cstring>
#include "m0rtis/m0rtis.h"

void test_version() {
    
    assert(std::strcmp(m0rtis::VERSION, "0.0.0") == 0);

}

int main(void) {
    test_version();
    return 0;

}

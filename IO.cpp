#include <iostream>
#include <stdint.h>

extern "C" {
    int32_t readval();
}

extern "C" {
    int32_t printval(int32_t v);
}

int32_t n;

int32_t readval() {
   std::cout << "Inserisci il valore di n: ";
   std::cin >> n;
   return n;
}

int32_t printval(int32_t v) {
    std::cout << "funfact(" << n << ")=" << v << std::endl;
    return 0;
}



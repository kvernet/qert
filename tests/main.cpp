#include "gates.hpp"
#include "statevector.hpp"

#include <bitset>
#include <iostream>

int main()
{
    void print(const qert::Statevector &sv);
    uint32_t num_qubits = 1;
    qert::Statevector sv(num_qubits);
    //sv.amplitude(0) = qert::Complex{1, 1};
    //sv.amplitude(1) = qert::Complex{-2, 3};

    print(sv);

    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    print(sv);

    return 0;
}


std::string to_binary(uint64_t x, uint32_t bits) {
    std::string s(bits, '0');

    for (int j = bits - 1; j >= 0; --j) {
        s[j] = (x & 1ULL) ? '1' : '0';
        x >>= 1ULL;
    }

    return s;
}
void print(const qert::Statevector &sv) {
    uint64_t num_qubits = sv.num_qubits();
    uint64_t size = 1ULL << num_qubits;
    for (uint64_t i = 0; i < size; ++i) {
        std::cout << sv.amplitude(i) << 
        "|" + to_binary(i, num_qubits) <<
        ">";
        if (i < size - 1) {
            std::cout << " + ";
        }
    }
    std::cout << "" << std::endl;
}
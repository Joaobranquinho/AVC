#include "Golomb.hpp"
#include <math.h>
#include <tuple>
#include <bitset>
#include <string.h>

using namespace std;

Golomb::Golomb(): m(1){
    b = ceil(log2(m));
    t = std::pow(2,b) - m;
}

void Golomb::set_m(uint32_t m){
    this->m = m;
    b = ceil(log2(m));
    t = std::pow(2,b) - m;
}

void Golomb::encode_and_write(int number, WRITEBits & w){
    uint32_t new_number = 0;
    uint32_t shift = log2(this->m);

    if(number >= 0)
        new_number = number * 2;
    else
        new_number = -2*number-1;

    uint32_t q = (uint32_t) floor( (float) new_number/(float) this->m );

    for( uint32_t i = 0; i < q; i++){
        w.writeBits(1);
    }
    w.writeBits(0);

    uint32_t r = new_number - q*this->m;

    // TODO
    //if( (this->m & -(this->m)) == this->m)
    //  std::tie(r,shift) = truncatedBinary(r);

    //ret = ret << shift;
    //ret = ret | r;

    //uint32_t number_of_bits = q + 1 + shift;

    w.preWrite(r, shift);

}

std::tuple<uint32_t,uint32_t> Golomb::truncatedBinary(uint32_t r){

    uint32_t bin;
    uint32_t shift;

    if( r < t){
        shift = b-1;
        bin = r;
    }else{
        shift = b;
        bin = r + t;
    }

    return std::make_tuple(bin, shift);
}

int Golomb::decode(READBits & r){
    /*
     * Ver:
     * https://w3.ual.es/~vruiz/Docencia/Apuntes/Coding/Text/03-symbol_encoding/09-Golomb_coding/index.html
     */
    //printf("Entrou no Golomb\n");

    uint32_t q = 0;
    int result  = 0;
    uint32_t bit = r.readBits();
    while(bit ==  1){
        bit = r.readBits();
        q++;
    }


    /*
     * Calculate r
     */

    uint32_t resto = 0;
    for (uint32_t i = 0; i < b; ++i)
    {
        resto = resto << 1 | r.readBits();
    }

    /*
     * Esquecer truncated binary
     */

    /*
    uint32_t resto = 0;
    for(uint32_t i = 0; i<b -1; i++){
        resto = resto << 1 | r.readBits();
        printf("Preso for %d\n", i);

    }
    printf("MEIO 2\n");

    if(resto < t){
        result = q * m + resto;
        printf("MEIO 3\n");
    }
    else {
        resto = resto << 1 | r.readBits();
        result = q * m + resto - t;
    }
    */
    result = q * m + resto;
    


    if(result % 2 == 0)
    {
        return  result / 2;
    }
    else{
        return  (result + 1) / (-2);
    }
}

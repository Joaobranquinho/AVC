#ifndef FSTREAMBITS_H
#define FSTREAMBITS_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>


using namespace std;
class Stream {};

class READBits: public Stream {

    private:
        char buff = 0;
        ifstream f;
        int shamnt = -1;

    public:
        READBits(const string & file) : f (file.c_str(), ios::binary){}

        /*
         * Função que lê de um ficheiro um byte e devolve o seu valor bit a bit
         *
         * */
        uint8_t readBits(){
            if(shamnt == -1){
                f.read(&buff, 1);
                shamnt = 7;
            }
            uint8_t val = buff >> shamnt & 1U;
            shamnt--;
            return val;
        }

        /*
         * bits: numero de bits que contêm informação, isto é que quero ler do ficheiro
         * Função que devolve um item (short) que contem informação nos seus bits mais significativos
         *
         * TODO:  maybe refactor these functions, they are very difficult to use
         *        ATTENTION that a refactor here may break code everywhere eles
         * */
        unsigned short readItem(uint32_t bits){
            unsigned short frame = 0;
            uint32_t i = 0; 
            while(i < bits){
                frame = frame <<1;
                frame = frame | readBits();
                i++;
            }
            if(i < 16){
                frame = (frame << 1) | 1U; 
                i++;
            }
            //frame = frame << (16-bits);
            while(i < 16){
                frame = frame << 1;
                i++;
            }
            return frame;
        }

        /*
         * Função que devolve a primeira linha do ficheiro, no nosso caso contem o header que é preciso
         *
         * */
        string readHeader(){
            string header;
            getline(f, header);
            return header;

        }
        int gcount(){
            return f.gcount();
        }

        void parse_header_pv( map<char,string> & header, string header_line,
                    int delimiter(int) = ::isspace){

            // string header_line = readHeader();
            string token;

            auto e=header_line.end();
            auto i=header_line.begin();

            while(i!=e){
                i=find_if_not(i,e, delimiter);
                if(i==e) break;
                auto j=find_if(i,e, delimiter);
                token = string(i,j);
                // cout << token << endl;
                if(token.at(0) == 'W'){
                    // cout << "Width " << token.substr(1) << endl;
                    header['W'] = token.substr(1);
                }
                else if(token.at(0) == 'H'){
                    // cout << "Height " << token.substr(1) <<endl;
                    header['H'] = token.substr(1);
                }
                else if(token.at(0) == 'K'){
                    // cout << "K " << token.substr(1) <<endl;
                    header['K'] = convertToASCII(token.substr(1));
                }
                else if(token.at(0) == 'S'){
                    // cout << "Seed " << token.substr(1) <<endl;
                    header['S'] = convertToASCII(token.substr(1));
                }
                else if(token.at(0) == 'C'){
                    // cout << "Colour Space " << token.substr(1) << endl;
                    header['C'] = token.substr(1);
                }
                i=j;
            }
            if (header.find('C') == header.end()){
                cout << "Colour Space " << token.substr(1) << endl;
                header['C'] = "420";
            }
        }

        string convertToASCII(string letter){
            string x;
            for (uint32_t i = 0; i < letter.length(); i++)
            {
                x+=to_string(int(letter.at(i)));
            }
            return x;
        }

        vector<uint32_t> read_header_cavlac(){
            vector<uint32_t> header_properties ( 5,0);

            // Read 32 bits
            uint32_t frames = readItem(16);
            frames = frames << 16 ;
            frames = frames | (readItem(16) & 0x0000FFFF);
            header_properties.at(0) = frames;

            // Read 32 bits
            uint32_t sample_rate = readItem(16);
            sample_rate = sample_rate << 16;
            sample_rate = sample_rate | (readItem(16) & 0x0000FFFF);
            header_properties.at(1) = sample_rate;

            // Read 16
            uint32_t channels= readItem(16);
            header_properties.at(2) = channels;


            // Read 32
            uint32_t format = readItem(16);
            format = format << 16;
            format = format | (readItem(16) & 0x0000FFFF);
            header_properties.at(3) = format;

            // read 16
            uint32_t block_size= readItem(16);
            header_properties.at(4) = block_size;

            return header_properties;
        }

        vector<uint32_t> reade_header_frame(){
            vector<uint32_t> header_properties ( 3 ,0);

            unsigned short  frame_header = readItem(8) >> 8;

            uint32_t constant = frame_header >> 6;
            uint32_t predictor = (frame_header >> 4) & 0x3;
            uint32_t best_k = frame_header & 0x0F;

            //printf(" Header read: %u, %u, %u", constant, predictor, best_k);

            header_properties.at(0) = constant;
            header_properties.at(1) = predictor;
            header_properties.at(2) = best_k;

            return header_properties;
        }
};

class WRITEBits: public Stream {
    private:
        char buff = 0;
        int shamnt = 7;
        ofstream f;

    public:

        /*
         * val: bit de informação que vai ser usado no byte a ser escrito
         * Função que escreve em um ficheiro um byte contendo apenas bits de informação
         *
         * */
        WRITEBits(const string & file) : f (file.c_str(), ios::binary){};

        void writeBits(char val){
            buff = buff | (val << shamnt);
            shamnt--;
            if(shamnt==-1){
                f.write( &buff, 1);
                shamnt = 7;
                buff = 0;
            }
        }

        /*
         * write: item que é preciso escrever para o ficheiro
         * bits: numero de bits que contêm informação no item "write"
         * Função que chama a "writeBits" para escrever no ficheiro
         *
         * */
        void preWrite(uint32_t write, uint32_t bits){
            int count = bits-1;
            while(count != -1){
                char val = (write >> count) & 1U;
                writeBits(val);
                count--;
            }

        }

        /*
         * Função que escreve o header no ficheiro
         *
         * */
        void writeHeader(uint32_t width, uint32_t height, uint32_t colorspace){
            f << "PARVUS" << " W" << width << " H" << height << " C" << colorspace << endl;
        }

        void writeFrameHeader(uint8_t k, uint8_t seed){
            f << "S" << seed << " K" << k << endl;
        }

        void flush(){
            while(shamnt != 7){
                writeBits(0);
            }
            f.close();
        }
};

#endif

#include "Golomb.hpp"
#include "Predictor.hpp"
#include "cxxopts.hpp"
#include "wavhist.h"

#include <iostream>
#include <tuple>
#include <math.h>
#include <bitset>
#include <vector>
#include <sndfile.hh>



using namespace std;

int decodeMode(string file);
int encodeMode(string file, int block_size, bool histogram);

int main(int argc, char *argv[]) {
    try {
        cxxopts::Options options("CAVLAC", "Lossless Audio Codec made for CAV");

        // Mode of operation of the codec
        // 0 - encode
        // 1 - decode
        int mode_operation = 0;
        int block_size = 0;
        string file;

        bool histogram;


        options.add_options()
            ("h,help", "Print help")
            ("f,file", "File (obrigatory)", cxxopts::value<std::string>())
            ("m,modeopps", "Mode of operation (obrigatory)", cxxopts::value(mode_operation))
            ("b,blocksize", "Block Size (needed when encoding)", cxxopts::value(block_size))
            ("H,histogram", "If present when executed the program will write the histograms of the residuals", cxxopts::value(histogram));

        auto result = options.parse(argc, argv);

        if (result.count("help")){
            cout << options.help() << endl;
            exit(0);
        }

        // Verify if a file was passed because its needed
        // in both modes of operation
        if (result.count("f") != 1){
            cout << endl << "You always need to specify a file" << endl << endl;
            cout << options.help() << endl;
            exit(1);
        }else{
            file = result["f"].as<string>();
        }

        if(result.count("m")){
            mode_operation = result["m"].as<int>();
            if (  !mode_operation ){

                // Encoding Mode
                if(result.count("b") != 1){
                    cout << endl << "In Encoding Mode you need to specify a block size" << endl << endl;
                    cout << options.help() << endl;
                    exit(1);
                }

                block_size = result["b"].as<int>();
                histogram = result["H"].as<bool>();

                exit(encodeMode(file, block_size, histogram));
            }else{
                // Decoding Mode
                exit(decodeMode(file));
            }
        }else{
            cout << options.help() << endl;
            exit(1);
        }

        cout << options.help() << endl;
        exit(0);
    }catch(const cxxopts::OptionException& e){
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }
    return 0;
}


int decodeMode(string file){
    cout << "decode" << endl;

    // TODO: verify if the file is a Cavlac one
    Golomb n();

    READBits w (file);

    // read header of file
    vector<uint32_t> properties = w.read_header_cavlac();

    for( int a: properties)
        cout << a << ";";

    // open an sndfile for writing
    // after having parameters from header of cavlac file
    SndfileHandle sndFileOut { file, SFM_WRITE, (int) properties.at(3),
        (int) properties.at(2), (int) properties.at(1) };

    // start reading frames



    return 0;
}

int encodeMode(string file, int block_size, bool histogram){
    cout << "encode" << endl;

    SndfileHandle sndFileIn { file };
    if(sndFileIn.error()) {
        cerr << "Error: invalid input file" << endl;
        return 1;
    }

    if((sndFileIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: file is not in WAV format" << endl;
        return 1;
    }

    if((sndFileIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: file is not in PCM_16 format" << endl;
        return 1;
    }

    WAVHist residuals_hist { sndFileIn, file, 4 };
    WAVHist hist { sndFileIn, file };

    vector<short> left_channel(block_size);
    vector<short> differences(block_size);

    size_t nFrames;
    vector<short> samples(block_size * 2);

    Golomb golo;

    WRITEBits w (file+".cavlac");
    // frames -> 32 bits -> 4 bytes
    // samplerate -> 32 bits -> 4 bytes
    // channels- > 16 bits -> 2 bytes
    // format -> 16 bits -> 2 bytes

    //TODO ver frames
    w.preWrite(sndFileIn.frames(), 32);
    w.preWrite(sndFileIn.samplerate(), 32);
    w.preWrite(sndFileIn.channels(), 16);
    w.preWrite(sndFileIn.format(), 32);
    printf("%04x\n", sndFileIn.format());
    w.preWrite(block_size, 16);
    cout << "Properties: " << sndFileIn.frames() << "," << sndFileIn.samplerate()
        << "," << sndFileIn.channels() << "," << sndFileIn.format() << "," << block_size << endl;

    // Codificar
    Predictor pr(4, block_size);
    while((nFrames = sndFileIn.readf(samples.data(), block_size))) {
        /*
         * Resize vector, because last block couldn't be multiple of block size
         */
        samples.resize(nFrames * 2);
        left_channel.resize(nFrames);
        differences.resize(nFrames);

        /*
         * Resize block size and vector of residuals on Predictor
         */
        pr.set_block_size_and_clean(left_channel.size());

        uint32_t index = 0, n = 0;
        for(auto s : samples) {
            index = n % sndFileIn.channels();
            if(index == 0) left_channel.at(n/2) = s;
            else differences.at((n-1)/2) = left_channel.at((n-1)/2) - s;
            n++;
        }

        // Generate The residuals
        pr.populate_v(left_channel);
        vector<short> predictor_settings = pr.get_best_predictor_settings();

        /*  
         * Debug prints
         */
        //cout << "Constant Or Not: " << predictor_settings.at(2)<<endl;
        //cout << "best k: " << predictor_settings.at(1)<<endl;
        //cout << "Predictor Used: " << predictor_settings.at(0) <<endl;
        uint8_t constant = predictor_settings.at(2);
        uint8_t best_k = predictor_settings.at(1);
        uint8_t predictor_used = predictor_settings.at(0);

        /*
        * Write Frame Header of left channel 
        */
        uint32_t write_header = constant;
        write_header = write_header << 2;
        write_header = write_header | predictor_used;
        write_header = write_header << 4;
        write_header = write_header | best_k;

        //cout << "Header: " << hex << write_header << endl;

        uint32_t m = pow(2,best_k);

        //cout << "m: " << m << endl;

        golo.set_m( m );
        w.preWrite(write_header, 8);

        vector<short> residuals;


        /* 
         * If constant samples only need write one frame
         */
        if (constant == 1)
        {
            // cout << "Foi constante no left" << endl;
            w.preWrite(left_channel.at(0), 16);
        } else {
            residuals = pr.get_residuals(predictor_used);
            uint32_t i = 0;
            for (i = 0; i < predictor_used; ++i)
            {
                w.preWrite(left_channel.at(i), 16);
            }
            for (i = predictor_used; i < residuals.size(); ++i)
            {
                golo.encode_and_write(residuals.at(i), w);
            }
        }
        if(histogram){
            hist.simple_update_index(0, pr.get_residuals(predictor_used));
            residuals_hist.simple_update_index(0, pr.get_residuals(0));
            residuals_hist.simple_update_index(2, pr.get_residuals(1));
            residuals_hist.simple_update_index(4, pr.get_residuals(2));
            residuals_hist.simple_update_index(6, pr.get_residuals(3));
        }


        /*
         * Clean averages vector, residuals vector
         */
        pr.set_block_size_and_clean(differences.size());
        pr.populate_v(differences);


        /*
        * Write Frame Header of differences
        */
        constant = predictor_settings.at(2);
        best_k = predictor_settings.at(1);
        predictor_used = predictor_settings.at(0);

        write_header = constant;
        write_header = write_header << 2;
        write_header = write_header | predictor_used;
        write_header = write_header << 4;
        write_header = write_header | best_k;

        //cout << "Header: " << hex << write_header << endl;

        m = pow(2,best_k);

        //cout << "m: " << m << endl;
        golo.set_m( m );

        w.preWrite(write_header, 8);

        /* 
         * If constant samples only need write one frame
         */
        if (constant == 1)
        {
            // cout << "Foi constante nas samples" << endl;
            w.preWrite(differences.at(0), 16);
        } else {            
            residuals = pr.get_residuals(predictor_used);
            uint32_t i = 0;
            for (i = 0; i < predictor_used; ++i)
            {
                w.preWrite(differences.at(i), 16);
            }
            for (i = predictor_used; i < residuals.size(); ++i)
            {
                golo.encode_and_write(residuals.at(i), w);
            }
        }
        if(histogram){
            hist.simple_update_index(1, pr.get_residuals(predictor_used));
            residuals_hist.simple_update_index(1, pr.get_residuals(0));
            residuals_hist.simple_update_index(3, pr.get_residuals(1));
            residuals_hist.simple_update_index(5, pr.get_residuals(2));
            residuals_hist.simple_update_index(7, pr.get_residuals(3));
        }


    }
    w.flush();
    if(histogram){
        hist.full_dump();
        residuals_hist.full_dump();
    }
    return 0;
}

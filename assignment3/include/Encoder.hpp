#ifndef ENCODER_H
#define ENCODER_H

#include "Golomb.hpp"
#include "Frame.hpp"

class Encoder {

    private:
        ifstream infile;
        WRITEBits w;

        int get_best_k( vector<int> * residuals, int frame, int tck );

        void parse_header(map<char,string> & header, string header_line, int delimiter(int) = ::isspace);

        int get_residuals_from_matrix(cv::Mat * matrix, vector<int> * residuals);
        int get_residual_uniform( uint8_t pixel_value, uint8_t real_pixel_value);
        int get_residual_LOCO( uint8_t pixel_A, uint8_t pixel_B, uint8_t pixel_C,uint8_t real_pixel_value);


    public:

        Encoder(const string & in_file, const string & out_file);

        /*  Function used to encode and write N Frames */
        void encode_and_write_frame( Frame * frame , int f_counter , Golomb * g);

        /* Function used to encode and write */
        void encode_and_write();
};

#endif

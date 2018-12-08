#include "Encoder.hpp"


#include <cstdio>
#include <numeric>
#include "Frame.hpp"

Encoder::Encoder(const string & in_file, const string & out_file):
    infile(in_file.c_str()),w(out_file.c_str()){}

int WriteFile(std::string fname, std::map<int,int> *m) {
    int count = 0;
    if (m->empty())
        return 0;

    FILE *fp = fopen(fname.c_str(), "w");
    if (!fp)
        return -errno;

    for(std::map<int, int>::iterator it = m->begin(); it != m->end(); it++) {
        fprintf(fp, "%d %d\n", it->first, it->second);
        count++;
    }

    fclose(fp);
    return count;
}

double Encoder::get_best_k( vector<int> * residuals , int frame){

    double average = std::accumulate( residuals->begin(), residuals->end(), 0.0)/residuals->size();

    double E=0;
    double inverse = 1.0 / static_cast<double>(residuals->size());

    std::map<int, int> freq;

    for(unsigned int i=0;i<residuals->size();i++){
      freq[residuals->at(i)]++;
      E+=pow(static_cast<double>(residuals->at(i)) - average, 2);
    }

    printf("%d", frame);
    string fname = "hist"+ to_string(frame);
    printf("%s\n", fname.c_str());
    WriteFile( fname, &freq);

    printf("Average -> %f Deviation-> %f\n", average, sqrt(inverse*E));
    return average;
}

int Encoder::get_residual_uniform( uint8_t previous_pixel_value, uint8_t real_pixel_value ){

    int residual = real_pixel_value - previous_pixel_value;
    return residual;

}

int Encoder::get_residual_LOCO( uint8_t pixel_A, uint8_t pixel_B, uint8_t pixel_C, uint8_t real_pixel_value){

    uint8_t pixel_prevision, maxAB, minAB;

    maxAB = std::max(pixel_A, pixel_B);
    minAB = std::min(pixel_A, pixel_B);

    if( pixel_C >= maxAB){
        pixel_prevision = minAB;
    }else if ( pixel_C <= minAB ){
        pixel_prevision = maxAB;
    }else{
        pixel_prevision = pixel_A + pixel_B - pixel_C;
    }

    return get_residual_uniform( pixel_prevision, real_pixel_value);
}


void Encoder::parse_header(  map<char,string> & header,
                    string header_line,
                    int delimiter(int)){

    string token;

    auto e=header_line.end();
    auto i=header_line.begin();

    while(i!=e){
        i=find_if_not(i,e, delimiter);
        if(i==e) break;
        auto j=find_if(i,e, delimiter);
        token = string(i,j);
        cout << token << endl;
        if(token.at(0) == 'W'){
            cout << "Width " << token.substr(1) << endl;
            header['W'] = token.substr(1);
        }
        else if(token.at(0) == 'H'){
            cout << "Height " << token.substr(1) <<endl;
            header['H'] = token.substr(1);
        }
        else if(token.at(0) == 'F'){
            cout << "Frame Rate " << token.substr(1) << endl;
            header['F'] = token.substr(1);
        }
        else if(token.at(0) == 'I'){
            cout << "Interlacing not parsed" << endl;
        }
        else if(token.at(0) == 'A'){
            cout << "Aspect Ratio not parsed" << endl;
        }
        else if(token.at(0) == 'C'){
            cout << "Colour Space " << token.substr(1) << endl;
            header['C'] = token.substr(1);
        }
        i=j;
    }
    if (header.find('C') == header.end()){
        cout << "Colour Space " << token.substr(1) << endl;
        header['C'] = "420";
    }
}

void Encoder::encode_and_write_frame(Frame * frame, int f_counter){

    int mini_block_size = frame->get_y().cols;
    int mini_x, mini_y;
    vector<int> residuals;

    /* encode the Luminance Matrix */
    uint8_t seed = frame->get_y().at<uint8_t>(0,0);
    uint8_t last_real = seed;
    int residual;

    for( mini_y = 0; mini_y < mini_block_size; mini_y++){
        if( mini_y == 0 ){
            // First row
            for( mini_x = 1; mini_x < mini_block_size; mini_x++){
                residuals.push_back(get_residual_uniform(last_real, frame->get_y().at<uint8_t>(mini_x,mini_y)));
                last_real = frame->get_y().at<uint8_t>(mini_x,mini_y);
            }
        }else{
            // Other rows
            residuals.push_back(get_residual_uniform(frame->get_y().at<uint8_t>(0,mini_y-1), frame->get_y().at<uint8_t>(0,mini_y)));
            for( mini_x = 1; mini_x < mini_block_size; mini_x++){
                residual = get_residual_LOCO(
                        frame->get_y().at<uint8_t>(mini_x-1,mini_y),
                        frame->get_y().at<uint8_t>(mini_x,mini_y-1),
                        frame->get_y().at<uint8_t>(mini_x-1,mini_y-1),
                        frame->get_y().at<uint8_t>(mini_x,mini_y));

                residuals.push_back(residual);
            }
        }
    }
    get_best_k(&residuals, f_counter);
    printf("Done \n");
};

void Encoder::encode_and_write(){
    string line;
    int cols, rows,frame_counter =0;
    vector<unsigned char> imgData;
    Frame * f;

    getline(this->infile, line);

    map<char, string> header;

    parse_header(header, line);

    cols = stoi(header['W']);
    rows = stoi(header['H']);


    // printf("Writing Header To Compressed File...");
    this->w.writeHeader(cols,rows,stoi(header['C']));

    switch(stoi(header['C'])){
        case 444:{
            f = new Frame444 (rows, cols);
            imgData.resize(cols * rows * 3);
            break;
        }
        case 422:{
            f = new Frame422 (rows, cols);
            imgData.resize(cols * rows * 2);
            break;
        }
        case 420:{
            f = new Frame420 (rows, cols);
            imgData.resize(cols * rows * 3/2);
            break;
        }
        default:
            exit(1);
    }

    f->print_type();

    while(1){
        getline (this->infile,line); // Skipping word FRAME
        this->infile.read((char *) imgData.data(), imgData.size());
        f->set_frame_data(imgData.data());
        if(this->infile.gcount() == 0){
            break;
        }
        encode_and_write_frame(f, frame_counter);
        frame_counter +=1;
    }

    delete f;
};



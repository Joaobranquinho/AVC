#include <iostream>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <sndfile.hh>
#include "wavhist.h"

#include <iterator>
#include <numeric>
#include <algorithm>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

double euclid_distance_squared(vector<short> &v1, vector<short> &v2){

  double dist = 0.0;
  double d;

  for( unsigned f = 0; f < v1.size(); f++){
    d = v1[f] - v2[f];
    dist += d*d;
  }

  return dist;

}

double avg_distortion_c0( vector<short> &c0, vector<vector<short>> &dataset){

  vector<double> distances;

  for ( unsigned i = 0; i< dataset.size(); i++){
    distances.push_back(euclid_distance_squared(c0, dataset[i]));
  }

  return  accumulate( distances.begin(), distances.end(), 0.0)/distances.size();
}


double avg_distortion_c_list( vector<vector<short>> &c_list, vector<vector<short>> &dataset){

  vector<double> distances;
  int index = 0;

  for ( auto  &c_i : c_list ){
    distances.push_back(euclid_distance_squared(c_i,dataset[index]));
    index ++;
  }

  return  accumulate( distances.begin(), distances.end(), 0.0)/distances.size();

}

vector<short> avg_vec_of_vecs(vector<vector<short>> &cluster){

  int size = cluster.size();
  vector<short> avg (cluster[0].size(), 0);
  int index;

  for ( auto const& block: cluster){
    index = 0;
    for ( auto const &value : block ){
      avg[index] += value / size;

    }
  }

  return avg;
}

vector<short> new_block(vector<short> &block, int move_distance){

  vector<short> new_block (block.size(), 0);
  for ( unsigned i = 0; i < block.size(); i++){
    new_block[i] = block[i]*( 1 + move_distance);
  }

  return new_block;
}

void split_codebook(
    vector<vector<short>> &dataset,
    vector<vector<short>> &cb
    , double epsilon
    , double initial_avg_dist
    ,vector<int> &abs_weights
    ,vector<float> &rel_weights
    , double &avg_dist ){

  vector<vector<short>> new_blocks;
  vector<short> c1;
  vector<short> c2;
  for ( auto &block : cb ){
    c1 = new_block(block, epsilon);
    c1 = new_block(block, -epsilon);
    new_blocks.push_back(c1);
    new_blocks.push_back(c2);
  }

  cb = new_blocks;

  int len_codebook = cb.size();

  abs_weights.resize(len_codebook, 0);
  rel_weights.resize(len_codebook, 0.0);

  cout << "new size: " << len_codebook << endl;

  avg_dist = 0;
  double err = epsilon + 1;
  int num_iter = 0;


  vector<vector<short>> closest_c_list (dataset.size());
  map<int, vector<vector<short>>> vecs_near_c;
  map<int, vector<int>> vec_idxs_near_c;

  while (err>epsilon){
    int indexA = 0;
    for ( auto &vec : dataset ){

      int first_time = 1;
      int min_dist;
      int closest_c_index;

      int index = 0;
      for ( auto &c : cb ){
        int d = euclid_distance_squared(vec,c);
        if ( first_time || d < min_dist){
          first_time = 0;
          min_dist = d;
          closest_c_list[index] = c;
          closest_c_index = index;
        }

        index++;
      }

      vecs_near_c[closest_c_index].push_back(vec);
      vec_idxs_near_c[closest_c_index].push_back(indexA);
      indexA ++;
    }

    // Update codebook
    int num_vecs_near_c;
    vector<short> new_c;
    for ( int i_c = 0; i_c < signed(cb.size()); i_c++){
      vector<vector<short>> vecs;
      try {
        vecs = vecs_near_c.at(i_c);
        num_vecs_near_c = vecs.size();
      }
      catch (const std::out_of_range& e) {
        num_vecs_near_c = 0;
      }

      if ( num_vecs_near_c > 0) {
        new_c = avg_vec_of_vecs(vecs);
        cb.at(i_c) = new_c;

        for( int j = 0; j < signed(vec_idxs_near_c.size()); j++){
          closest_c_list[j] = new_c;
        }

        abs_weights[i_c] = num_vecs_near_c;
        rel_weights[i_c] = num_vecs_near_c/dataset.size();
      }
    }

    int prev_avg_dist;
    if (avg_dist > 0)
      prev_avg_dist = avg_dist;
    else
      prev_avg_dist = initial_avg_dist;


    avg_dist = avg_distortion_c_list(closest_c_list, dataset);

    err = ( prev_avg_dist - avg_dist) / prev_avg_dist;

    num_iter += 1;

  }
}


vector<vector<short>> generate_codebook(vector<vector<short>> dataset, int size, int epsilon=0.00001){

  vector<vector<short>> codebook;

  vector<short> c0 = avg_vec_of_vecs(dataset);

  codebook.push_back(c0);

  double avg_dist_initial = avg_distortion_c0(c0, dataset);

  vector<int> codebook_abs_weights;
  vector<float> codebook_rel_weights;

  double avg_dist;

  while(signed(codebook.size()) < size){
    split_codebook( dataset,
                    codebook,
                    epsilon,
                    avg_dist_initial,
                    codebook_abs_weights,
                    codebook_rel_weights,
                    avg_dist);

  }

  return codebook;
}

int main(int argc, char *argv[]) {

  if(argc < 5) {
    cerr << "Usage: wavhist <input file> <block size> <overlap size> <codebook size> " << endl;
    return 1;
  }

  SndfileHandle sndFile { argv[1] };
  if(sndFile.error()) {
    cerr << "Error: invalid input file" << endl;
    return 1;
  }

  if((sndFile.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
    cerr << "Error: file is not in WAV format" << endl;
    return 1;
  }

  if((sndFile.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
    cerr << "Error: file is not in PCM_16 format" << endl;
    return 1;
  }


  size_t nFrames;
  vector<vector<short>> dataSet;
  vector<vector<short>> codebook;

  vector<short> samples(FRAMES_BUFFER_SIZE * sndFile.channels());
  while((nFrames = sndFile.readf(samples.data(), 4))) {
    samples.resize(8);
    vector<short> A = {samples[0], samples[2], samples[4], samples[6]};
    vector<short> B = {samples[1], samples[3], samples[5], samples[7]};

    dataSet.push_back(A);
    dataSet.push_back(B);
  }

  cout << "create cb" << endl;

  //cout << sndFile.frames();
  //for(int i = 0; i < stoi(argv[4]); i++){
  //  codebook.push_back(dataSet[rand() % dataSet.size()]);
  //}
  //

  codebook = generate_codebook(dataSet, 1024);

  ofstream outFile("cb.txt");
  for( const auto &e : codebook){
    for ( const auto &f : e){
      outFile << f;
      outFile << " " ;
    }
    outFile << endl;
  }

  cout << "get v" << endl;




  ofstream outFile2("cb2.txt");
  for( const auto &e : codebook){
    for ( const auto &f : e){
      outFile2 << f;
      outFile2 << " " ;
    }
    outFile2 << endl;
  }

  //SndfileHandle sndFileN { argv[1] };

  //SndfileHandle sndFileOut;
  //sndFileOut = SndfileHandle("sampleOutVector.wav", SFM_WRITE, sndFile.format(), sndFile.channels(), sndFile.samplerate());

  //short frame [2];
  //int channels = sndFile.channels();

  //vector<short> samples2(8);
  //while((nFrames = sndFileN.readf(samples2.data(), 4))) {
  //  vector<short> A = {samples2[0], samples2[2], samples2[4], samples2[6]};
  //  vector<short> B = {samples2[1], samples2[3], samples2[5], samples2[7]};
  //  int idxA = getBestMatchingUnit(codebook, A);
  //  int idxB = getBestMatchingUnit(codebook, B);

  //  frame[0] = codebook[idxA][0];
  //  frame[1] = codebook[idxB][0];
  //  sndFileOut.writef(frame, (sizeof(frame)*8)/16/channels);
  //  frame[0] = codebook[idxA][1];
  //  frame[1] = codebook[idxB][1];
  //  sndFileOut.writef(frame, (sizeof(frame)*8)/16/channels);
  //  frame[0] = codebook[idxA][2];
  //  frame[1] = codebook[idxB][2];
  //  sndFileOut.writef(frame, (sizeof(frame)*8)/16/channels);
  //  frame[0] = codebook[idxA][3];
  //  frame[1] = codebook[idxB][3];
  //  sndFileOut.writef(frame, (sizeof(frame)*8)/16/channels);


  //}


  //cout << dataSet.size() << endl;
  //cout << sndFile.frames();

  return 0;
}

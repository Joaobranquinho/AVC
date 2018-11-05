#include "Predictor.hpp"

#include <algorithm>
#include <map>
#include <math.h>
#include <vector>
#include <numeric>


using namespace std;

Predictor::Predictor(uint32_t max_order, uint32_t block_size) :
    max_order(max_order),
    block_size(block_size),
    averages (max_order, 0),
    block_all_residuals(max_order, vector<int> (block_size))
{
}

void Predictor::set_block_size_and_clean(uint32_t size){
    this->block_size = size;
    this->block_all_residuals.clear();
    this->block_all_residuals.resize(this->max_order, vector<int>(size,0));
    this->averages = vector<double> (this->max_order, 0);
}

void Predictor::populate_v(vector<int> & samples) {
    for (uint32_t i = this->block_size - 1; i > 1; i--){
        gen_residuals( samples, i, max_order-1);
    }

    //print_matrix(this->block_all_residuals);
}

void Predictor::print_matrix( vector<vector<int>> & matrix){
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t max_order = matrix.size();
    uint32_t block_size = matrix.at(0).size();

    for (i = 0; i < max_order; i++) {
        for (j = 0; j < block_size; j++){
            cout << matrix.at(i).at(j) << ", ";
        }
        cout << endl;
    }

}


int Predictor::gen_residuals(vector<int> & samples, uint32_t index, uint32_t order) {

    int rn = 0;

    if(block_all_residuals.at(order).at(index) != 0) return block_all_residuals.at(order).at(index);
    else if (order == 0) rn = samples.at(index);
    else if (index < order) rn = 0; 
    else rn = gen_residuals(samples, index, order-1) - gen_residuals(samples, index - 1, order-1);

    this->block_all_residuals.at(order).at(index) = rn;

    if ( rn < 0 )
        this->averages.at(order) = this->averages.at(order) + (rn * -1);
    else
        this->averages.at(order) = this->averages.at(order) + rn ;

    return rn;
}


vector<float> Predictor::calculate_entropies(){

    vector<float> entropies (this->max_order, 0);

    float entropy = 0;

    map<short,long> counts;
    map<short,long>::iterator it;

    for( uint32_t i = 0; i < this->max_order; i++){

      entropy = 0;

      for (uint32_t residual_index = 0; residual_index < this->block_size; residual_index++) {
        counts[this->block_all_residuals.at(i)[residual_index]]++;
      }

      it = counts.begin();
      while(it != counts.end()){
        float p_x = (float)it->second/this->block_size;
        if (p_x>0) entropy-=p_x*log(p_x)/log(2);
          it++;
      }

      entropies.at(i) = entropy;
      counts.clear();
    }
    return entropies;
};

vector<double> Predictor::get_averages(){

    for (uint32_t i = 0; i < this->max_order; ++i){
        averages.at(i) = averages.at(i) / this->block_size;
    }

    return this->averages;
};

vector<short> Predictor::get_best_predictor_settings(){
    vector<short> settings( 3, 0);

    vector<double> averages = get_averages();
    vector<double>::iterator result = min_element(begin(averages), end(averages));
    short minimum_median_index = distance(begin(averages), result);
    settings.at(0)= minimum_median_index;
    if(*result < 1){
        settings.at(1)= 1;
    }
    else{
        settings.at(1)= ceil(log2(*result));
    }
    /*if(ceil(log2(*result)) >= 16 || ceil(log2(*result)) < 0){
        cout << "BELA MERDA" << endl;
        cout << settings.at(1) << endl;
        cout << *result << endl;
    
    }*/
    settings.at(2)= averages.at(1) == (double)0.0 ? 1 : 0 ;

    return settings;
}

vector<int> Predictor::get_residuals(uint32_t predictor_index){
    return this->block_all_residuals.at(predictor_index);
}

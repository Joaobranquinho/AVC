#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <iostream>
#include <string.h>
#include <vector>
#include "fstreamBits.h"

using namespace std;

class Predictor {

    private:
        uint32_t max_order, block_size;
        vector<vector<short>> block_all_residuals ;

        // Entropy Calculation
        // It will also be able to output histogram data
        // because the histogram calculations are the same
        // we must calculate the frequency of each residual
        // TODO: save_freqs
        vector<float> calculate_entropies(bool save_freqs);

        // Average Calculation
        // Calculates the Average of the saved residuals
        // Average is usefull because the value can be used
        // to set the m on Golomb coding
        vector<double> calculate_averages(bool save_freqs);

    public:

        // Constructor
        Predictor(uint32_t max_order, uint32_t block_size);

        // Function that will build de matrix with all the residuals
        void populate_v(vector<short> & samples);

        // Recursive function that generates the residual values
        // up to max_order
        short gen_residuals(vector<short> & samples, uint32_t index, uint32_t order);

        // Get Id of the best predictor setting
        // 0 - id of the best predictor
        // 1 - value for m in GOlomb encoding
        // 2 - is frame constant
        vector<short> get_best_predictor_settings(uint32_t mode);

        // Get residual values of the best predictor
        vector<short> get_residuals(uint32_t predictor_index);

        // Helper Functions

        // Print a matrix
        void print_matrix( vector<vector<short>> & matrix );
};

#endif
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>

// g++ -std=c++11 -o readfloats readfloats.cpp

// ----- Simple Linear Regression (C function style) -----
int linreg_simple(const double *x, const double *y, size_t n,
                  double *slope, double *intercept, double *r2) {
    double sx=0, sy=0, sxx=0, sxy=0, syy=0;
    for(size_t i=0; i<n; i++) {
        sx  += x[i];
        sy  += y[i];
        sxx += x[i]*x[i];
        sxy += x[i]*y[i];
        syy += y[i]*y[i];
    }
    double n_d = (double)n;
    double den = n_d*sxx - sx*sx;
    if(den == 0) return -1; // all x equal?

    *slope     = (n_d*sxy - sx*sy) / den;
    *intercept = (sy - (*slope)*sx) / n_d;

    if(r2) {
        double ss_tot = syy - (sy*sy)/n_d;
        double ss_res = ss_tot - (*slope)*(n_d*sxy - sx*sy)/n_d;
        *r2 = (ss_tot==0) ? 1.0 : 1.0 - ss_res/ss_tot;
    }
    return 0;
}

// ----- Main Program -----
int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " datafile\n";
        return 1;
    }

    std::ifstream infile(argv[1]);
    if(!infile) {
        std::cerr << "Error opening file " << argv[1] << "\n";
        return 1;
    }

    // read floats into a vector first
    std::vector<double> xv;
	std::vector<double> yv;
	
    float tx, ty;
	
    while(infile >> tx) {

		xv.push_back(tx);
		
		infile >> ty;
		
		yv.push_back(ty);
    }
	
    infile.close();

	/*
    if(xv.size() % 2 != 0) {
        std::cerr << "Error: file must contain pairs of floats (x y)\n";
        return 1;
    }
	*/

    size_t n = xv.size();

	/*
    // create float arrays for x and y
    float* xf = xv.data();  // contiguous memory from vector
	float* yf = yv.data();
	
    std::vector<double> xd(n), yd(n); // convert to double for regression
    for(size_t i=0; i<n; i++) {
        xd[i] = xf[i];
        yd[i] = yf[i];
    }
	*/

    double slope, intercept, r2;
    if(linreg_simple(xv.data(), yv.data(), n, &slope, &intercept, &r2) == 0) {
        std::cout << "Linear regression result:\n";
        std::cout << "y = " << intercept << " + " << slope << " * x\n";
        std::cout << "R^2 = " << r2 << "\n";
    } else {
        std::cerr << "Regression failed (maybe all x were identical)\n";
    }

    return 0;
}
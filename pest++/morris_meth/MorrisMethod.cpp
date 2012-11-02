#include <iostream>
#include <vector>
#include <Eigen/Dense>
#include <cassert>
#include "MorrisMethod.h"
#include "Transformable.h"
#include "RunManagerAbstract.h"
#include "ParamTransformSeq.h"

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

MatrixXd MorrisMethod::create_P_star_mat(int k)
{
	MatrixXd b_mat = create_B_mat(k);
	MatrixXd j_mat = create_J_mat(k);
	MatrixXd d_mat = create_D_mat(k);
	MatrixXd x_vec = create_x_vec(k);
	MatrixXd p_mat = create_P_mat(k);
	b_star_mat = j_mat.col(0) * x_vec.transpose() + (delta/2.0)*((2.0 * b_mat - j_mat) * d_mat + j_mat) ;
	return b_star_mat;
}

MorrisMethod::MorrisMethod(const vector<string> &_par_name_vec, const Parameters &_lower_bnd, 
		const Parameters &_upper_bnd, int _p)
		: par_name_vec(_par_name_vec), lower_bnd(_lower_bnd), upper_bnd(_upper_bnd), p(_p)

{
	// p should be even.  If not, add 1 to it.
	if (p % 2 != 0) {
		p +=1;
	}
	delta = p / (2.0 * (p - 1));
}

MatrixXd MorrisMethod::create_B_mat(int k)
{
	MatrixXd b_mat = MatrixXd::Constant(k+1, k, 0.0);
	int nrow = k+1;
	int ncol = k;
	for (int icol=0; icol<ncol; ++icol)
	{
		for (int irow=icol+1; irow<nrow; ++irow)
		{
			b_mat(irow, icol) = 1.0;
		}
	}
	return b_mat;
}

MatrixXd MorrisMethod::create_J_mat(int k)
{
	MatrixXd j_mat = MatrixXd::Constant(k+1, k, 1.0);
	return j_mat;
}

MatrixXd MorrisMethod::create_D_mat(int k)
{
	MatrixXd d_mat=MatrixXd::Constant(k, k, 0.0);
	for (int i=0; i<k; ++i) {
		d_mat(i,i) = rand_plus_minus_1();
	}
	return d_mat;
}

MatrixXd MorrisMethod::create_P_mat(int k)
{
	// generate and return a random permutation matrix
	MatrixXd p_mat=MatrixXd::Constant(k, k, 0.0);
	int ird;
	int itmp;
	vector<int> rand_idx;
	rand_idx.reserve(k);
	for (int i=0; i<k; ++i) {
		rand_idx.push_back(i);
	}
	// Shuffle random index vector
	for (int i=0; i<k; ++i) {
		ird = rand();
		ird = ird % k;
		itmp = rand_idx[i];
		rand_idx[i] = rand_idx[ird];
		rand_idx[ird] = itmp;
	}

	for(int irow=0; irow<k; ++irow)
	{
	  p_mat(irow, rand_idx[irow]) = 1;
	}

	return p_mat;
}

VectorXd MorrisMethod::create_x_vec(int k)
{
	// Warning Satelli's book is wrong.  Need to use Morris's original paper
	// to compute x.  Maximim value of any xi shoud be 1.0 - delta.
	VectorXd x(k);
	double rnum;
	// Used with the randon number generator below, max_num will produce a
	// random interger which varies between 0 and (p-2)/2.  This will in turn
	// produce the correct xi's which vary from {0, 1/(p-1), 2/(p-1), .... 1 - delta)
	// when divided by (p-1).  See Morris's paper for the derivaition.
	// Satelli's book has this equation wrong!
	int max_num = 1+(p-2)/2;
	for (int i=0; i<k; ++i)
	{
		rnum = rand() % max_num;
		x(i) = rnum / (p - 1);
	}
	return x;
}


int MorrisMethod::rand_plus_minus_1(void)
{
	return (rand() % 2 * 2) - 1 ;
}


Parameters MorrisMethod::get_ctl_parameters(int row)
{
	Parameters ctl_pars;
	auto e=par_name_vec.end();
	int n_cols = b_star_mat.cols();
	for (int j=0; j<n_cols; ++j)
	{
		const string &p = par_name_vec[j];
		auto it_lbnd = lower_bnd.find(p);
		assert(it_lbnd != lower_bnd.end());
		auto it_ubnd = upper_bnd.find(p);
		assert(it_ubnd != upper_bnd.end());

		ctl_pars[p] =  it_lbnd->second + (it_ubnd->second - it_lbnd->second) * b_star_mat(row, j);
	}
	return ctl_pars;
}

void MorrisMethod::assemble_runs(RunManagerAbstract &rm, ParamTransformSeq &ctl2model_tran)
{
	b_star_mat = create_P_star_mat(par_name_vec.size());
    int n_rows = b_star_mat.rows();
	for (int i=0; i<n_rows; ++i)
	{
        Parameters pars = get_ctl_parameters(i);
        rm.add_run(pars);
	}
}

MorrisMethod::~MorrisMethod(void)
{
}

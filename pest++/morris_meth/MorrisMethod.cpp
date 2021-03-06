#include <vector>
#include <Eigen/Dense>
#include <cassert>
#include <numeric>
#include <math.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <regex>
#include "MorrisMethod.h"
#include "Transformable.h"
#include "RunManagerAbstract.h"
#include "ParamTransformSeq.h"
#include "ModelRunPP.h"
#include "utilities.h"
#include "FileManager.h"
#include "Stats.h"

using namespace std;
using namespace pest_utils;
using Eigen::MatrixXd;
using Eigen::VectorXd;

void MorrisObsSenFile::initialize(const vector<string> &_par_names_vec, const vector<string> &_obs_names_vec, double _no_data, const GsaAbstractBase *_gsa_abstract_base)
{
	par_names_vec =_par_names_vec;
	obs_names_vec = _obs_names_vec;
	gsa_abstract_base = _gsa_abstract_base;
	no_data = _no_data;
}

void MorrisObsSenFile::add_sen_run_pair(const std::string &par_name, Parameters &par1, Observations &obs1, Parameters &par2, Observations &obs2)
{
	assert (obs1.size() == obs2.size());
	assert (par1.size() == par2.size());
	int nobs = obs1.size();

	// compute sensitivities of individual observations
	double isen;
	double del_par = par2[par_name] - par1[par_name];
	if (gsa_abstract_base->is_log_trans_par(par_name))
	{
		del_par =  log10(par2[par_name]) - log10(par1[par_name]);
	}
	for (const auto &iobs : obs_names_vec)
	{
		isen = (obs2[iobs] - obs1[iobs]) / del_par;
		const auto id = make_pair(par_name, iobs);
		if (map_obs_stats.find(id) == map_obs_stats.end())
		{
			map_obs_stats[id] = RunningStats();
		}
		map_obs_stats[make_pair(par_name, iobs)].add(isen);
	}
}

void MorrisObsSenFile::calc_obs_sen(ofstream &fout_obs_sen)
{

	fout_obs_sen << "par_name, obs_name, mean, abs_mean, sigma, n_samples" << endl;  
	for (const auto &ipar : par_names_vec)
	{
		for(const auto &iobs : obs_names_vec)
		{
			double mean = no_data;
			double abs_mean = no_data;
			double sigma = no_data;
			int n_samples = 0;
			const auto id = make_pair(ipar, iobs);
			auto sen_itr = map_obs_stats.find(id);
			if (sen_itr != map_obs_stats.end() && sen_itr->second.comp_nsamples() > 0)
			{
				mean = sen_itr->second.comp_mean();
				abs_mean = sen_itr->second.comp_abs_mean();
				sigma = sen_itr->second.comp_sigma();
				mean = sen_itr->second.comp_mean();
				n_samples = sen_itr->second.comp_nsamples();
			}
			string parname = gsa_abstract_base->log_name(ipar);
			fout_obs_sen << parname << ", " << iobs << ", " << mean << ", " << abs_mean << ", " << sigma <<", " << n_samples <<  endl;
		}
	}
}


void MorrisMethod::process_pooled_var_file(std::ifstream &fin)
{
	string line;
	regex reg_reg ("regex\\s*\\(\"(.+)\"\\)");
	regex reg_grp("group\\s*\\((.+)\\)");
	cmatch mr;
	while (getline(fin, line))
	{
		cout << line << endl;
		if (regex_match(line.c_str(), mr, reg_reg))
		{
			for (auto x : mr) cout << x << " ";
			cout << std::endl;
		}
		if (regex_match(line.c_str(), mr, reg_grp))
		{
			for (auto x : mr) cout << x << " ";
			cout << std::endl;
		}
	}
}

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

MorrisMethod::MorrisMethod(const vector<string> &_adj_par_name_vec,  const Parameters &_fixed_pars,
						   const Parameters &_lower_bnd, const Parameters &_upper_bnd, const set<string> &_log_trans_pars, 
			   int _p, int _r, RunManagerAbstract *_rm_ptr, ParamTransformSeq *_base_partran_seq_ptr, 
			   const std::vector<std::string> &_obs_name_vec, FileManager *file_manager_ptr)
						   : GsaAbstractBase(_rm_ptr, _base_partran_seq_ptr, _adj_par_name_vec, _fixed_pars, _lower_bnd, _upper_bnd, 
						   _obs_name_vec, file_manager_ptr)
{
	initialize(_log_trans_pars, _p, _r);
}


void MorrisMethod::initialize(const set<string> &_log_trans_pars, 
						   int _p, int _r)
{
	log_trans_pars = _log_trans_pars;
	p = _p;
	r = _r;

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
	vector<int> rand_idx;
	rand_idx.reserve(k);
	for (int i=0; i<k; ++i) {
		rand_idx.push_back(i);
	}
	// Shuffle random index vector
	random_shuffle(rand_idx.begin(), rand_idx.end());
	//for (int i=0; i<k; ++i) {
		//ird = rand();
		//ird = ird % k;
		//itmp = rand_idx[i];
		//rand_idx[i] = rand_idx[ird];
		//rand_idx[ird] = itmp;
	//}

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
	auto e=adj_par_name_vec.end();
	int n_cols = b_star_mat.cols();
	for (int j=0; j<n_cols; ++j)
	{
		const string &p = adj_par_name_vec[j];
		auto it_lbnd = lower_bnd.find(p);
		assert(it_lbnd != lower_bnd.end());
		auto it_ubnd = upper_bnd.find(p);
		assert(it_ubnd != upper_bnd.end());
		if (log_trans_pars.find(p) != log_trans_pars.end())
		{
			double temp = b_star_mat(row, j);
			double update = log10(it_lbnd->second) + (log10(it_ubnd->second) - log10(it_lbnd->second)) * b_star_mat(row, j);
			ctl_pars[p] =  pow(10.0, update);
		}
		else 
		{
			ctl_pars[p] =  it_lbnd->second + (it_ubnd->second - it_lbnd->second) * b_star_mat(row, j);
		}
	}
	return ctl_pars;
}

void MorrisMethod::assemble_runs()
{
	runid_2_parname_map.clear();
	for (int tmp_r=0; tmp_r<r; ++tmp_r)
	{
		b_star_mat = create_P_star_mat(adj_par_name_vec.size());
		int n_rows = b_star_mat.rows();
		int run_id;
		string *par_name = nullptr;
		double par_value;
		for (int i=0; i<n_rows; ++i)
		{
			//get control parameters
			Parameters pars = get_ctl_parameters(i);
			pars.insert(fixed_ctl_pars.begin(), fixed_ctl_pars.end());
			// converst control parameters to model parameters
			base_partran_seq_ptr->ctl2model_ip(pars);
			run_id = run_manager_ptr->add_run(pars);
			par_name = nullptr;
			par_value = pars.no_data;
			if (i>0)
			{
				par_name = &adj_par_name_vec[i-1];
				par_value = pars.get_rec(*par_name);
			}
			runid_2_parname_map[run_id] = par_name;
		}
	}
}

void  MorrisMethod::calc_sen(ModelRun model_run, ofstream &fout_raw, ofstream &fout_morris, ofstream &fout_orw)
{
	ModelRun run0 = model_run;
	ModelRun run1 = model_run;
	Parameters pars0;
	Observations obs0;
	Parameters pars1;
	Observations obs1;
	string *p;
	unsigned int n_adj_par = adj_par_name_vec.size();
	map<string, RunningStats > sen_map;

	for (auto &it_p : adj_par_name_vec)
	{
		sen_map[it_p] = RunningStats();
	}


	fstream &fout_mbn = file_manager_ptr->open_iofile_ext("mbn", ios::out | ios::in | ios::binary);
	obs_sen_file.initialize(adj_par_name_vec, run_manager_ptr->get_obs_name_vec(), Observations::no_data, this);

	fout_raw << "parameter_name, phi_0, phi_1, par_0, par_1, sen" << endl;

	int n_runs = run_manager_ptr->get_nruns();
	bool run0_ok, run1_ok;
	for (int i_run=1; i_run<n_runs; ++i_run)
	{
		run0_ok = run_manager_ptr->get_run(i_run-1, pars0, obs0);
		run1_ok = run_manager_ptr->get_run(i_run, pars1, obs1);
		auto it = runid_2_parname_map.find(i_run);
		assert(it != runid_2_parname_map.end());
		p = it->second;
		if (run0_ok && run1_ok && p!=nullptr)
		{
			run0.update_ctl(pars0, obs0);
			double phi0 = run0.get_phi(0.0);
			run1.update_ctl(pars1, obs1);
			double phi1 = run1.get_phi(0.0);
			double p0 = pars0[*p];
			double p1 = pars1[*p];
			if (log_trans_pars.find(*p) != log_trans_pars.end())
			{
				p0 = log10(p0);
				p1 = log10(p1);
			}
			// compute standard Morris Sensitivity on the global objective function
			double sen = (phi1 - phi0) / delta;
			fout_raw << log_name(*p) << ",  " <<  phi1 << ",  " << phi0 << ",  " << p1 << ",  " << p0 << ", " << sen << endl;

			const auto &it_senmap = sen_map.find(*p);
			if (it_senmap != sen_map.end())
			{
				it_senmap->second.add(sen);
			}

			//Compute sensitvities of indiviual observations
			obs_sen_file.add_sen_run_pair(log_name(*p), pars0, obs0, pars1, obs1);
		}
	}
	ofstream &fout_mos = file_manager_ptr->open_ofile_ext("mos");
	obs_sen_file.calc_obs_sen(fout_mos);
	file_manager_ptr->close_file("mos");
	// write standard Morris Sensitivity on the global objective function
	fout_morris << "parameter_name, sen_mean, sen_mean_abs, sen_std_dev" << endl;
	for (const auto &it_par : adj_par_name_vec)
	{
		const auto &it_senmap = sen_map.find(it_par);
		if (it_senmap != sen_map.end())
		{
			fout_morris << log_name(it_par) << ", " << it_senmap->second.comp_mean() << ", " << it_senmap->second.comp_abs_mean() << ", " <<  it_senmap->second.comp_var() << endl;
		}
	}
}

MorrisMethod::~MorrisMethod(void)
{
}

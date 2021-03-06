/*  
	� Copyright 2012, David Welter
	
	This file is part of PEST++.
   
	PEST++ is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	PEST++ is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with PEST++.  If not, see<http://www.gnu.org/licenses/>.
*/
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include "SVDSolver.h"
#include "RunManagerAbstract.h"
#include "QSqrtMatrix.h"
#include "eigen_tools.h"
#include "ObjectiveFunc.h"
#include "utilities.h"
#include "FileManager.h"
#include "TerminationController.h"
#include "ParamTransformSeq.h"
#include "Transformation.h"
#include "PriorInformation.h"
#include "Regularization.h"
#include "SVD_PROPACK.h"
#include "OutputFileWriter.h"
#include <sstream>

using namespace std;
using namespace pest_utils;
using namespace Eigen;

SVDSolver::SVDSolver(const ControlInfo *_ctl_info, const SVDInfo &_svd_info, const ParameterGroupInfo *_par_group_info_ptr, const ParameterInfo *_ctl_par_info_ptr,
		const ObservationInfo *_obs_info, FileManager &_file_manager, const Observations *_observations, ObjectiveFunc *_obj_func,
		const ParamTransformSeq &_par_transform, const PriorInformation *_prior_info_ptr, Jacobian &_jacobian, 
		const DynamicRegularization *_regul_scheme_ptr, OutputFileWriter &_output_file_writer, RestartController &_restart_controller, SVDSolver::MAT_INV _mat_inv,
		PerformanceLog *_performance_log, const string &_description)
		: ctl_info(_ctl_info), svd_info(_svd_info), par_group_info_ptr(_par_group_info_ptr), ctl_par_info_ptr(_ctl_par_info_ptr), obs_info_ptr(_obs_info), obj_func(_obj_func),
		  file_manager(_file_manager), observations_ptr(_observations), par_transform(_par_transform),
		  cur_solution(_obj_func, *_observations), phiredswh_flag(false), save_next_jacobian(true), prior_info_ptr(_prior_info_ptr), jacobian(_jacobian), prev_phi_percent(0.0),
		  num_no_descent(0), regul_scheme(*_regul_scheme_ptr), output_file_writer(_output_file_writer), mat_inv(_mat_inv), description(_description), best_lambda(20.0),
		  restart_controller(_restart_controller), performance_log(_performance_log)
{
	svd_package = new SVD_EIGEN();
}

void SVDSolver::set_svd_package(PestppOptions::SVD_PACK _svd_pack)
{
	if(_svd_pack == PestppOptions::PROPACK){
		delete svd_package;
		svd_package = new SVD_PROPACK;
	}
	else {
		delete svd_package;
		svd_package = new SVD_EIGEN;
	}
	svd_package->set_max_sing(svd_info.maxsing);
	svd_package->set_eign_thres(svd_info.eigthresh);
}


SVDSolver::~SVDSolver(void)
{
	delete svd_package;
}


ModelRun& SVDSolver::solve(RunManagerAbstract &run_manager, TerminationController &termination_ctl, int max_iter, 
	ModelRun &cur_run, ModelRun &optimum_run)
{
	ostream &os = file_manager.rec_ofstream();
	ostream &fout_restart = file_manager.get_ofstream("rst");
	cur_solution = cur_run;
	// Start Solution iterations
	bool save_nextjac = false;
	string matrix_inv = (mat_inv == MAT_INV::Q12J) ? "\"Q 1/2 J\"" : "\"Jt Q J\"";
	for (int iter_num=1; iter_num<=max_iter;++iter_num) {
		int global_iter_num = termination_ctl.get_iteration_number()+1;
		cout << "OPTIMISATION ITERATION NUMBER: " << global_iter_num << endl;
		os   << "OPTIMISATION ITERATION NUMBER: " << global_iter_num << endl << endl;
		cout << "  Iteration type: " << get_description() << endl;
		os   << "    Iteration type: " << get_description() << endl;
		cout << "  SVD Package: " << svd_package->description << endl;
		os   << "    SVD Package: " << svd_package->description << endl;
		cout << "  Matrix Inversion: " << matrix_inv << endl;
		os   << "    Matrix Inversion: " << matrix_inv << endl;
		os   << "    Model calls so far : " << run_manager.get_total_runs() << endl;
		fout_restart << "start_iteration " << iter_num << "  " << global_iter_num << endl;
		cout << endl;
		os << endl;

		// write head for SVD file
		output_file_writer.write_svd_iteration(global_iter_num);

		performance_log->log_blank_lines();
		performance_log->add_indent(-10);
		ostringstream tmp_str;
		tmp_str << "beginning iteration " << global_iter_num;
		performance_log->log_event(tmp_str.str(), 0, "start_iter");
		performance_log->add_indent();
		iteration(run_manager, termination_ctl, false);
		tmp_str.str("");
		tmp_str.clear();
		tmp_str << "completed iteration " << global_iter_num;
		performance_log->log_event(tmp_str.str(), 0, "end_iter");
		tmp_str.str("");
		tmp_str.clear();
		tmp_str << "time to complete iteration " << global_iter_num;
		performance_log->log_summary(tmp_str.str(), "end_iter", "start_iter");
		// write files that get wrtten at the end of each iteration
		stringstream filename;
		string complete_filename;

		// rei file for this iteration
		filename << "rei" << global_iter_num;
		output_file_writer.write_rei(file_manager.open_ofile_ext(filename.str()), global_iter_num, 
			*(cur_solution.get_obj_func_ptr()->get_obs_ptr()), 
			cur_solution.get_obs(), *(cur_solution.get_obj_func_ptr()),
			cur_solution.get_ctl_pars());
		file_manager.close_file(filename.str());
		// par file for this iteration
		output_file_writer.write_par(file_manager.open_ofile_ext("par"), cur_solution.get_ctl_pars(), *(par_transform.get_offset_ptr()),
			*(par_transform.get_scale_ptr()));
		file_manager.close_file("par");

		filename.str(""); // reset the stringstream
		filename << "par" << global_iter_num;
		output_file_writer.write_par(file_manager.open_ofile_ext(filename.str()), cur_solution.get_ctl_pars(), *(par_transform.get_offset_ptr()), 
				*(par_transform.get_scale_ptr()));
		file_manager.close_file(filename.str());
		// sen file for this iteration
		output_file_writer.append_sen(file_manager.sen_ofstream(), global_iter_num, jacobian, *(cur_solution.get_obj_func_ptr()), get_parameter_group_info());
		if (save_nextjac) {
			jacobian.save();
		}
		if (!optimum_run.obs_valid() || cur_solution.get_phi() < optimum_run.get_phi())
		{
			optimum_run.set_ctl_parameters(cur_solution.get_ctl_pars());
			optimum_run.set_observations(cur_solution.get_obs());
			// save new optimum parameters to .par file
			output_file_writer.write_par(file_manager.open_ofile_ext("bpa"), optimum_run.get_ctl_pars(), *(par_transform.get_offset_ptr()), 
				*(par_transform.get_scale_ptr()));
			file_manager.close_file("bpa");
			// save new optimum residuals to .rei file
			output_file_writer.write_rei(file_manager.open_ofile_ext("rei"), global_iter_num, 
			*(optimum_run.get_obj_func_ptr()->get_obs_ptr()), 
			optimum_run.get_obs(), *(optimum_run.get_obj_func_ptr()),
			optimum_run.get_ctl_pars());
			file_manager.close_file("rei");
			jacobian.save();
			// jacobian calculated next iteration will be at the current parameters and
			// will be more accurate than the one caluculated at the begining of this iteration
			save_nextjac = true;
		}
		os << endl << endl;
		if (termination_ctl.check_last_iteration()){
			break;
		}
	}
	return cur_solution;
}

VectorXd SVDSolver::calc_residual_corrections(const Jacobian &jacobian, const Parameters &del_numeric_pars, 
							   const vector<string> obs_name_vec)
{
	VectorXd del_residuals;
	if (del_numeric_pars.size() > 0)
	{
		vector<string>frz_par_name_vec = del_numeric_pars.get_keys();
		//remove the parameters for which the jaocbian could not be computed
		const set<string> &failed_jac_par_names = jacobian.get_failed_parameter_names();
		auto end_iter = remove_if(frz_par_name_vec.begin(), frz_par_name_vec.end(),
			[&failed_jac_par_names](string &str)->bool{return failed_jac_par_names.find(str) != failed_jac_par_names.end(); });
		frz_par_name_vec.resize(std::distance(frz_par_name_vec.begin(), end_iter));

		VectorXd frz_del_par_vec = del_numeric_pars.get_data_eigen_vec(frz_par_name_vec);

		MatrixXd jac_frz = jacobian.get_matrix(obs_name_vec, frz_par_name_vec);
		del_residuals = (jac_frz)*  frz_del_par_vec;
	}
	else
	{
		del_residuals = VectorXd::Zero(obs_name_vec.size());
	}
	return del_residuals;
}

void SVDSolver::calc_lambda_upgrade_vec_JtQJ(const Jacobian &jacobian, const QSqrtMatrix &Q_sqrt,
	const Eigen::VectorXd &Residuals, const vector<string> &obs_name_vec,
	const Parameters &base_active_ctl_pars, const Parameters &prev_frozen_active_ctl_pars,
	double lambda, Parameters &active_ctl_upgrade_pars, Parameters &upgrade_active_ctl_del_pars,
	Parameters &grad_active_ctl_del_pars, MarquardtMatrix marquardt_type)
{
	Parameters base_numeric_pars = par_transform.active_ctl2numeric_cp(base_active_ctl_pars);
	//Create a set of Derivative Parameters which does not include the frozen Parameters
	Parameters pars_nf = base_active_ctl_pars;
	pars_nf.erase(prev_frozen_active_ctl_pars);
	//Transform these parameters to numeric parameters
	par_transform.active_ctl2numeric_ip(pars_nf);
	vector<string> numeric_par_names = pars_nf.get_keys();

	//Compute effect of frozen parameters on the residuals vector
	Parameters delta_freeze_pars = prev_frozen_active_ctl_pars;
	Parameters base_freeze_pars(base_active_ctl_pars, delta_freeze_pars.get_keys());
	par_transform.ctl2numeric_ip(delta_freeze_pars);
	par_transform.ctl2numeric_ip(base_freeze_pars);
	delta_freeze_pars -= base_freeze_pars;
	VectorXd del_residuals = calc_residual_corrections(jacobian, delta_freeze_pars, obs_name_vec);

	VectorXd Sigma;
	VectorXd Sigma_trunc;
	Eigen::SparseMatrix<double> U;
	Eigen::SparseMatrix<double> Vt;

	Eigen::SparseMatrix<double> q_mat = Q_sqrt.get_sparse_matrix(obs_name_vec);
	q_mat = (q_mat * q_mat).eval();
	Eigen::SparseMatrix<double> jac = jacobian.get_matrix(obs_name_vec, numeric_par_names);
	Eigen::SparseMatrix<double> ident;
	ident.resize(jac.cols(), jac.cols());
	ident.setIdentity();
	Eigen::SparseMatrix<double> JtQJ = jac.transpose() * q_mat * jac;
	//Compute Scaling Matrix Sii
	performance_log->log_event("commencing to scale JtQJ matrix");
	svd_package->solve_ip(JtQJ, Sigma, U, Vt, Sigma_trunc, 0.0);

	VectorXd Sigma_inv_sqrt = Sigma.array().inverse().sqrt();
	VectorXd Sigma_sqrt = Sigma.array().sqrt();
	Eigen::SparseMatrix<double> S = Vt.transpose() * Sigma_inv_sqrt.asDiagonal() * U.transpose();
	Eigen::SparseMatrix<double> S_inv = Vt.transpose() * Sigma_sqrt.asDiagonal() * U.transpose();
	JtQJ = (jac * S).transpose() * q_mat * jac * S;
	performance_log->log_event("scaling of  JtQJ matrix complete");
	if (marquardt_type == MarquardtMatrix::IDENT)
	{
		JtQJ += lambda * S.transpose() * S;
	}
	else
	{
		VectorXd diag = lambda * JtQJ.diagonal();
		MatrixXd diag_mat = diag.asDiagonal();
		JtQJ = (JtQJ + diag_mat.sparseView());
	}
	// Returns truncated Sigma, U and Vt arrays with small singular parameters trimed off
	performance_log->log_event("commencing SVD factorization");
	svd_package->solve_ip(JtQJ, Sigma, U, Vt, Sigma_trunc);
	performance_log->log_event("SVD factorization complete");

	output_file_writer.write_svd(Sigma, Vt, lambda, prev_frozen_active_ctl_pars, Sigma_trunc);

	VectorXd Sigma_inv = Sigma.array().inverse();
	
	performance_log->log_event("commencing linear algebra multiplication to compute ugrade");
	Eigen::VectorXd upgrade_vec;
	upgrade_vec = S * (Vt.transpose() * (Sigma_inv.asDiagonal() * (U.transpose() * ((jac * S).transpose()* (q_mat  * (Residuals + del_residuals))))));

	Eigen::VectorXd grad_vec;
	grad_vec = -2.0 * (jac.transpose() * (q_mat * Residuals));
	performance_log->log_event("linear algebra multiplication to compute ugrade complete");

	//tranfere newly computed componets of the ugrade vector to upgrade.svd_uvec
	upgrade_active_ctl_del_pars.clear();
	grad_active_ctl_del_pars.clear();

	string *name_ptr;
	auto it_nf_end = pars_nf.end();
	for (int i = 0; i<numeric_par_names.size(); ++i)
	{
		name_ptr = &(numeric_par_names[i]);
		upgrade_active_ctl_del_pars[*name_ptr] = upgrade_vec(i);
		grad_active_ctl_del_pars[*name_ptr] = grad_vec(i);
		auto it_nf = pars_nf.find(*name_ptr);
		if (it_nf != it_nf_end)
		{
			it_nf->second += upgrade_vec(i);
		}
	}
	// Transform upgrade_pars back to derivative parameters
	active_ctl_upgrade_pars = par_transform.numeric2active_ctl_cp(pars_nf);
	par_transform.del_numeric_2_del_active_ctl_ip(upgrade_active_ctl_del_pars, Parameters(base_numeric_pars));
	par_transform.del_numeric_2_del_active_ctl_ip(grad_active_ctl_del_pars, Parameters(base_numeric_pars));

	//tranfere previously frozen componets of the ugrade vector to upgrade.svd_uvec
	for (auto &ipar : prev_frozen_active_ctl_pars)
	{
		active_ctl_upgrade_pars[ipar.first] = ipar.second;
	}
}


void SVDSolver::calc_lambda_upgrade_vecQ12J(const Jacobian &jacobian, const QSqrtMatrix &Q_sqrt,
	const Eigen::VectorXd &Residuals, const vector<string> &obs_name_vec,
	const Parameters &base_active_ctl_pars, const Parameters &prev_frozen_active_ctl_pars,
	double lambda, Parameters &active_ctl_upgrade_pars, Parameters &upgrade_active_ctl_del_pars,
	Parameters &grad_active_ctl_del_pars, MarquardtMatrix marquardt_type)
{
	Parameters base_numeric_pars = par_transform.active_ctl2numeric_cp(base_active_ctl_pars);
	//Create a set of Ctl Parameters which does not include the frozen Parameters
	Parameters pars_nf = base_active_ctl_pars;
	pars_nf.erase(prev_frozen_active_ctl_pars);
	//Transform these parameters to numeric parameters
	par_transform.active_ctl2numeric_ip(pars_nf);
	vector<string> numeric_par_names = pars_nf.get_keys();

	//Compute effect of frozen parameters on the residuals vector
	Parameters delta_freeze_pars = prev_frozen_active_ctl_pars;
	Parameters base_freeze_pars(base_active_ctl_pars, delta_freeze_pars.get_keys());
	par_transform.active_ctl2numeric_ip(delta_freeze_pars);
	par_transform.active_ctl2numeric_ip(base_freeze_pars);
	delta_freeze_pars -= base_freeze_pars;
	VectorXd del_residuals = calc_residual_corrections(jacobian, delta_freeze_pars, obs_name_vec);

	VectorXd Sigma;
	VectorXd Sigma_trunc;
	Eigen::SparseMatrix<double> U;
	Eigen::SparseMatrix<double> Vt;
	Eigen::SparseMatrix<double> q_sqrt = Q_sqrt.get_sparse_matrix(obs_name_vec);
	Eigen::SparseMatrix<double> jac = jacobian.get_matrix(obs_name_vec, numeric_par_names);
	Eigen::SparseMatrix<double> SqrtQ_J = q_sqrt * jac;
	// Returns truncated Sigma, U and Vt arrays with small singular parameters trimed off
	performance_log->log_event("commencing SVD factorization");
	svd_package->solve_ip(SqrtQ_J, Sigma, U, Vt, Sigma_trunc);
	performance_log->log_event("SVD factorization complete");
	//Only add lambda to singular values above the threshhold 
	if (marquardt_type == MarquardtMatrix::IDENT)
	{
		Sigma = Sigma.array() + lambda;
	}
	else
	{
		//this needs checking 
		Sigma = Sigma.array() + (Sigma.cwiseProduct(Sigma).array() * lambda).sqrt();
	}
	output_file_writer.write_svd(Sigma, Vt, lambda, prev_frozen_active_ctl_pars, Sigma_trunc);
	VectorXd Sigma_inv = Sigma.array().inverse();

	performance_log->log_event("commencing linear algebra multiplication to compute ugrade");
	Eigen::VectorXd upgrade_vec;
	upgrade_vec = Vt.transpose() * (Sigma_inv.asDiagonal() * (U.transpose() * (q_sqrt  * (Residuals + del_residuals))));

	Eigen::VectorXd grad_vec;
	grad_vec = -2.0 * (jac.transpose() * (q_sqrt * (q_sqrt * Residuals)));
	performance_log->log_event("linear algebra multiplication to compute ugrade complete");

	//tranfere newly computed componets of the ugrade vector to upgrade.svd_uvec
	upgrade_active_ctl_del_pars.clear();
	grad_active_ctl_del_pars.clear();
	
	string *name_ptr;
	auto it_nf_end = pars_nf.end();
	for (int i = 0; i<numeric_par_names.size(); ++i)
	{
		name_ptr = &(numeric_par_names[i]);
		upgrade_active_ctl_del_pars[*name_ptr] = upgrade_vec(i);
		grad_active_ctl_del_pars[*name_ptr] = grad_vec(i);
		auto it_nf = pars_nf.find(*name_ptr);
		if (it_nf != it_nf_end)
		{
			it_nf->second += upgrade_vec(i);
		}
	}
	// Transform upgrade_pars back to ctl parameters
	active_ctl_upgrade_pars = par_transform.numeric2active_ctl_cp(pars_nf);
	par_transform.del_numeric_2_del_active_ctl_ip(upgrade_active_ctl_del_pars, Parameters(base_numeric_pars));
	par_transform.del_numeric_2_del_active_ctl_ip(grad_active_ctl_del_pars, Parameters(base_numeric_pars));

	//tranfere previously frozen componets of the ugrade vector to upgrade.svd_uvec
	for (auto &ipar : prev_frozen_active_ctl_pars)
	{
		active_ctl_upgrade_pars[ipar.first] = ipar.second;
	}
}

void SVDSolver::calc_upgrade_vec(double i_lambda, Parameters &prev_frozen_active_ctl_pars, QSqrtMatrix &Q_sqrt, VectorXd &residuals_vec,
	vector<string> &obs_names_vec, const Parameters &base_run_active_ctl_pars, LimitType &limit_type, Parameters &upgrade_active_ctl_pars, MarquardtMatrix marquardt_type)
{
	Parameters upgrade_ctl_del_pars;
	Parameters grad_ctl_del_pars;
	int num_upgrade_out_grad_in;
	Parameters new_frozen_active_ctl_pars;

	upgrade_active_ctl_pars.clear();
	// define a function type for upgrade methods 
	typedef void(SVDSolver::*UPGRADE_FUNCTION) (const Jacobian &jacobian, const QSqrtMatrix &Q_sqrt,
		const Eigen::VectorXd &Residuals, const vector<string> &obs_name_vec,
		const Parameters &base_ctl_pars, const Parameters &prev_frozen_ctl_pars,
		double lambda, Parameters &ctl_upgrade_pars, Parameters &upgrade_ctl_del_pars,
		Parameters &grad_ctl_del_pars, MarquardtMatrix marquardt_type);

	UPGRADE_FUNCTION calc_lambda_upgrade = &SVDSolver::calc_lambda_upgrade_vec_JtQJ;

	if (mat_inv == MAT_INV::Q12J)
	{
		calc_lambda_upgrade = &SVDSolver::calc_lambda_upgrade_vecQ12J;
	}


	// need to remove parameters frozen due to failed jacobian runs when calling calc_lambda_upgrade_vec
	//Freeze Parameters at the boundary whose ugrade vector and gradient both head out of bounds
	(*this.*calc_lambda_upgrade)(jacobian, Q_sqrt, residuals_vec, obs_names_vec,
		base_run_active_ctl_pars, prev_frozen_active_ctl_pars, i_lambda, upgrade_active_ctl_pars, upgrade_ctl_del_pars,
		grad_ctl_del_pars, marquardt_type);
	num_upgrade_out_grad_in = check_bnd_par(new_frozen_active_ctl_pars, base_run_active_ctl_pars, upgrade_ctl_del_pars, grad_ctl_del_pars);
	prev_frozen_active_ctl_pars.insert(new_frozen_active_ctl_pars.begin(), new_frozen_active_ctl_pars.end());
	//Recompute the ugrade vector without the newly frozen parameters and freeze those at the boundary whose upgrade still goes heads out of bounds
	if (num_upgrade_out_grad_in > 0)
	{
		new_frozen_active_ctl_pars.clear();
		(*this.*calc_lambda_upgrade)(jacobian, Q_sqrt, residuals_vec, obs_names_vec,
			base_run_active_ctl_pars, prev_frozen_active_ctl_pars, i_lambda, upgrade_active_ctl_pars, upgrade_ctl_del_pars,
			grad_ctl_del_pars, marquardt_type);
		check_bnd_par(new_frozen_active_ctl_pars, prev_frozen_active_ctl_pars, upgrade_active_ctl_pars);
		prev_frozen_active_ctl_pars.insert(new_frozen_active_ctl_pars.begin(), new_frozen_active_ctl_pars.end());
		new_frozen_active_ctl_pars.clear();
	}
	//If there are newly frozen parameters recompute the upgrade vector
	if (new_frozen_active_ctl_pars.size() > 0)
	{
		(*this.*calc_lambda_upgrade)(jacobian, Q_sqrt, residuals_vec, obs_names_vec,
			base_run_active_ctl_pars, prev_frozen_active_ctl_pars, i_lambda, upgrade_active_ctl_pars, upgrade_ctl_del_pars,
			grad_ctl_del_pars, marquardt_type);
	}
	//Freeze any new parameters that want to go out of bounds
	new_frozen_active_ctl_pars.clear();
	new_frozen_active_ctl_pars = limit_parameters_freeze_all_ip(base_run_active_ctl_pars, upgrade_active_ctl_pars, prev_frozen_active_ctl_pars);
	prev_frozen_active_ctl_pars.insert(new_frozen_active_ctl_pars.begin(), new_frozen_active_ctl_pars.end());
}

void SVDSolver::iteration(RunManagerAbstract &run_manager, TerminationController &termination_ctl, bool calc_init_obs)
{
	ostream &os = file_manager.rec_ofstream();
	set<string> out_ofbound_pars;

	vector<string> obs_names_vec = cur_solution.get_obs_template().get_keys();
	vector<string> numeric_parname_vec = par_transform.ctl2numeric_cp(cur_solution.get_ctl_pars()).get_keys();

	//Save information necessary for restart
	ostream &fout_restart = file_manager.get_ofstream("rst");
	fout_restart << "base_par_iteration" << endl;

	if (restart_controller.get_restart_option() == RestartController::RestartOption::REUSE_JACOBIAN)
	{
		restart_controller.get_restart_option() = RestartController::RestartOption::NONE;
		cout << "  reading previosuly computed jacobian... ";
		{
			jacobian.read(file_manager.build_filename("jco"));
		}

		cout << endl << endl;
		cout << "  running the model once with the current parameters... ";
		run_manager.reinitialize(file_manager.build_filename("rnu"));
		int run_id = run_manager.add_run(par_transform.ctl2model_cp(cur_solution.get_ctl_pars()));
		run_manager.run();
		Parameters tmp_pars;
		Observations tmp_obs;
		bool success = run_manager.get_run(run_id, tmp_pars, tmp_obs);
		if (success)
		{
			par_transform.model2ctl_ip(tmp_pars);
			cur_solution.update_ctl(tmp_pars, tmp_obs);
			goto restart_reuse_jacoboian;
		}
		else
		{
			throw(PestError("Error: Base parameter run failed.  Can not continue."));
		}
	}
	else if (restart_controller.get_restart_option() == RestartController::RestartOption::RESUME_JACOBIAN_RUNS)
	{
		Parameters tmp_pars;
		ifstream &fin_par = file_manager.open_ifile_ext("rpb");
		output_file_writer.read_par(fin_par, tmp_pars);
		file_manager.close_file("rpb");
		cur_solution.set_ctl_parameters(tmp_pars);
		goto restart_resume_jacobian_runs;
	}

	// Calculate Jacobian
	if (!cur_solution.obs_valid() || calc_init_obs == true) {
		calc_init_obs = true;
	}
	cout << "  calculating jacobian... ";
	performance_log->log_event("commencing to build jacobian parameter sets");
	jacobian.build_runs(cur_solution, numeric_parname_vec, par_transform,
		*par_group_info_ptr, *ctl_par_info_ptr, run_manager, out_ofbound_pars,
		phiredswh_flag, calc_init_obs);
restart_resume_jacobian_runs:
	// save current parameters
	{
	  ofstream &fout_rpb = file_manager.open_ofile_ext("rpb");
	  output_file_writer.write_par(fout_rpb, cur_solution.get_ctl_pars(), *(par_transform.get_offset_ptr()),
		*(par_transform.get_scale_ptr()));
	  file_manager.close_file("rpb");
	}
	// save state of termination controller
	termination_ctl.save_state(fout_restart);

	performance_log->log_event("jacobian parameter sets built, commencing model runs");
	jacobian.make_runs(run_manager);
	performance_log->log_event("jacobian runs complete, processing runs");
	jacobian.process_runs(numeric_parname_vec, par_transform,
		*par_group_info_ptr, *ctl_par_info_ptr, run_manager, *prior_info_ptr, out_ofbound_pars,
		phiredswh_flag, calc_init_obs);
	performance_log->log_event("processing jacobian runs complete");
	//Update parameters and observations for base run
	{
		Parameters tmp_pars;
		Observations tmp_obs;
		bool success = run_manager.get_run(0, tmp_pars, tmp_obs);
		par_transform.model2ctl_ip(tmp_pars);
		cur_solution.update_ctl(tmp_pars, tmp_obs);
	}
	restart_reuse_jacoboian:
	cout << endl;


	//Freeze Parameter for which the jacobian could not be calculated
	auto &failed_jac_pars_names = jacobian.get_failed_parameter_names();
	auto  failed_jac_pars = cur_solution.get_ctl_pars().get_subset(failed_jac_pars_names.begin(), failed_jac_pars_names.end());

	// populate vectors with sorted observations (standard and prior info) and parameters
	{
		vector<string> prior_info_names = prior_info_ptr->get_keys();
		obs_names_vec.insert(obs_names_vec.end(), prior_info_names.begin(), prior_info_names.end());
	}

	// build weights matrix sqrt(Q)
	double tikhonov_weight = regul_scheme.get_weight();
	QSqrtMatrix Q_sqrt(obs_info_ptr, prior_info_ptr, tikhonov_weight);
	//build residuals vector
	VectorXd residuals_vec = -1.0 * stlvec_2_egienvec(cur_solution.get_residuals_vec(obs_names_vec));

	Parameters base_run_active_ctl_par = par_transform.ctl2active_ctl_cp(cur_solution.get_ctl_pars());
	vector<double> magnitude_vec;
	Parameters frozen_active_ctl_pars = failed_jac_pars;
	LimitType limit_type = LimitType::NONE;
	//If running in regularization mode, adjust the regularization weights
	// define a function type for upgrade methods 
	//dynamic_weight_adj(jacobian, Q_sqrt, residuals_vec, obs_names_vec,
	//	base_run_active_ctl_par, frozen_active_ctl_pars);
	//tikhonov_weight = Q_sqrt.get_tikhonov_weight();
	//regul_scheme.set_weight(tikhonov_weight);


	// write out report for starting phi
	obj_func->phi_report(os, cur_solution.get_obs(), cur_solution.get_ctl_pars(), tikhonov_weight);
	// write failed jacobian parameters out
	if (failed_jac_pars.size() > 0)
	{
		os << endl;
		os << "  the following parameters have been frozen as the runs to compute their derivatives failed: " << endl;
		for (auto &ipar : failed_jac_pars)
		{
			os << "    " << ipar.first << " frozen at " << ipar.second << endl;
		}
	}


	//Build model runs
	run_manager.reinitialize(file_manager.build_filename("rnu"));
	cout << endl;
	cout << "  computing upgrade vectors... " << endl;
	//Marquardt Lambda Update Vector
	double tmp_lambda[] = {0.1, 1.0, 10.0, 100.0, 1000.0};
	vector<double> lambda_vec(tmp_lambda, tmp_lambda+sizeof(tmp_lambda)/sizeof(double));
	lambda_vec.push_back(best_lambda);
	lambda_vec.push_back(best_lambda / 2.0);
	lambda_vec.push_back(best_lambda * 2.0);
	std::sort(lambda_vec.begin(), lambda_vec.end());
	auto iter = std::unique(lambda_vec.begin(), lambda_vec.end());
	lambda_vec.resize(std::distance(lambda_vec.begin(), iter));
	int i_update_vec = 0;
	stringstream message;
	vector<Parameters> frozen_par_vec;
	for (double i_lambda : lambda_vec)
	{
		std::cout << string(message.str().size(), '\b');
		message.str("");
		message << "  computing upgrade vector (lambda = " << i_lambda << ")  " << ++i_update_vec << " / " << lambda_vec.size() << "             ";
		std::cout << message.str();

		Parameters new_pars;
		calc_upgrade_vec(i_lambda, frozen_active_ctl_pars, Q_sqrt, residuals_vec,
			obs_names_vec, base_run_active_ctl_par, limit_type,
			new_pars, MarquardtMatrix::IDENT);

		magnitude_vec.push_back(Transformable::l2_norm(base_run_active_ctl_par, new_pars));
		par_transform.active_ctl2model_ip(new_pars);
		run_manager.add_run(new_pars, "IDEN", i_lambda);
		frozen_par_vec.push_back(frozen_active_ctl_pars);
	}

	cout << endl;
	fout_restart << "upgrade_model_runs_built " << run_manager.get_cur_groupid() << endl;
	cout << "  performing upgrade vector runs... ";
	run_manager.run();

	// process model runs
	cout << endl;
	cout << "  testing upgrade vectors... ";
	cout << endl;
	bool best_run_updated_flag = false;
	ModelRun best_upgrade_run(cur_solution);

	long jac_num_nonzero = jacobian.get_nonzero();
	long jac_num_total = jacobian.get_size();
	long jac_num_zero = jac_num_total - jac_num_nonzero;
	streamsize n_prec = os.precision(2);
	os << "    Number of terms in the jacobian equal to zero: " << jac_num_zero << " / " << jac_num_total
		<< " (" << double(jac_num_zero) / double(jac_num_total) * 100 << "%)" << endl << endl;
	os.precision(n_prec);

	os << "    Summary of upgrade runs:" << endl;
	for(int i=0; i<run_manager.get_nruns(); ++i) {
		ModelRun upgrade_run(cur_solution);
		Parameters tmp_pars;
		Observations tmp_obs;
		string lambda_type;
		double i_lambda;
		bool success = run_manager.get_run(i, tmp_pars, tmp_obs, lambda_type, i_lambda);
		if (success)
		{
			par_transform.model2ctl_ip(tmp_pars);
			upgrade_run.update_ctl(tmp_pars, tmp_obs);
			upgrade_run.set_frozen_ctl_parameters(frozen_par_vec[i]);
			streamsize n_prec = os.precision(2);
			os << "      Lambda = ";
			os << setiosflags(ios::fixed) << setw(8) << i_lambda;
			os << "; Type: " << setw(4) << lambda_type;
			os << "; length = " << magnitude_vec[i];
			os.precision(n_prec);
			os.unsetf(ios_base::floatfield); // reset all flags to default
			os << ";  phi = " << upgrade_run.get_phi(tikhonov_weight); 
			os.precision(2);
			os << setiosflags(ios::fixed);
			os << " ("  << upgrade_run.get_phi(tikhonov_weight)/cur_solution.get_phi(tikhonov_weight)*100 << "%)" << endl;
			os.precision(n_prec);
			os.unsetf(ios_base::floatfield); // reset all flags to default
			if (upgrade_run.obs_valid() && (!best_run_updated_flag || upgrade_run.get_phi(tikhonov_weight) <  best_upgrade_run.get_phi(tikhonov_weight))) {
				best_run_updated_flag = true;
				best_upgrade_run = upgrade_run;
				best_lambda = i_lambda;
			}
		}
		else
		{
			streamsize n_prec = os.precision(2);
			os << "      Marquardt Lambda = ";
			os << setiosflags(ios::fixed) << setw(4) << i_lambda;
			os << "; length = " << magnitude_vec[i];
			os.precision(n_prec);
			os.unsetf(ios_base::floatfield); // reset all flags to default
			os << ";  run failed" << endl;
		}
	}
	// Print frozen parameter information
	const Parameters &frz_ctl_pars = best_upgrade_run.get_frozen_ctl_pars();
		
	if (frz_ctl_pars.size() > 0)
	{
		vector<string> keys = frz_ctl_pars.get_keys();
		std::sort(keys.begin(), keys.end());
		os << endl;
		os << "    Parameters frozen during best upgrade:" << endl;
		for (auto &ikey : keys)
		{
			auto iter = frz_ctl_pars.find(ikey);
			if (iter != frz_ctl_pars.end())
			{
				os << "      " << iter->first << " frozen at " << iter->second << endl;
			}
		}
	}

	// clean up run_manager memory
	run_manager.free_memory();

	// reload best parameters and set flag to switch to central derivatives next iteration
	if (cur_solution.get_phi(tikhonov_weight) != 0 && !phiredswh_flag &&
		(cur_solution.get_phi(tikhonov_weight) - best_upgrade_run.get_phi(tikhonov_weight) / cur_solution.get_phi(tikhonov_weight) < ctl_info->phiredswh) )
	{
		phiredswh_flag = true;
		os << endl << "      Switching to central derivatives:" << endl;
	}

	cout << "  Starting phi = " << cur_solution.get_phi(tikhonov_weight) << ";  ending phi = " << best_upgrade_run.get_phi(tikhonov_weight) <<
		"  (" << best_upgrade_run.get_phi(tikhonov_weight) / cur_solution.get_phi(tikhonov_weight) * 100 << "%)" << endl;
	cout << endl;
	os << endl;
	iteration_update_and_report(os, best_upgrade_run, termination_ctl);
	prev_phi_percent = best_upgrade_run.get_phi(tikhonov_weight) / cur_solution.get_phi(tikhonov_weight) * 100;
	cur_solution = best_upgrade_run;
}


void SVDSolver::check_limits(const Parameters &init_active_ctl_pars, const Parameters &upgrade_active_ctl_pars,
						map<string, LimitType> &limit_type_map, Parameters &active_ctl_parameters_at_limit)
{
	const string *name;
	double p_init;
	double p_upgrade;
	double b_facorg_lim;
	pair<bool, double> par_limit;
	const ParameterRec *p_info;

	for (auto &ipar : upgrade_active_ctl_pars)
	{
		name = &(ipar.first);  // parameter name
		p_info = ctl_par_info_ptr->get_parameter_rec_ptr(*name);
		if (p_info->is_active())
		{
			par_limit = pair<bool, double>(false, 0.0);
			p_upgrade = ipar.second;  // upgrade parameter value
			p_init = init_active_ctl_pars.get_rec(*name);

			double init_value = ctl_par_info_ptr->get_parameter_rec_ptr(*name)->init_value;
			if (init_value == 0.0)
			{
				init_value = ctl_par_info_ptr->get_parameter_rec_ptr(*name)->ubnd / 4.0;
			}
			b_facorg_lim = ctl_info->facorig * init_value;
			if (abs(p_init) >= b_facorg_lim) {
				b_facorg_lim = p_init;
			}

			// Check Relative Chanage Limit
			if (p_info->chglim == "RELATIVE" && abs((p_upgrade - p_init) / b_facorg_lim) > ctl_info->relparmax)
			{
				par_limit.first = true;
				par_limit.second = p_init + sign(p_upgrade - p_init) * ctl_info->relparmax *  abs(b_facorg_lim);
				limit_type_map[*name] = LimitType::REL;
			}

			// Check Factor Change Limit
			else if (p_info->chglim == "FACTOR") {
				if (b_facorg_lim > 0 && p_upgrade < b_facorg_lim / ctl_info->facparmax)
				{
					par_limit.first = true;
					par_limit.second = b_facorg_lim / ctl_info->facparmax;
					limit_type_map[*name] = LimitType::FACT;
				}
				else if (b_facorg_lim > 0 && p_upgrade > b_facorg_lim*ctl_info->facparmax)
				{
					par_limit.first = true;
					par_limit.second = b_facorg_lim * ctl_info->facparmax;
					limit_type_map[*name] = LimitType::FACT;
				}
				else if (b_facorg_lim < 0 && p_upgrade < b_facorg_lim*ctl_info->facparmax)
				{
					par_limit.first = true;
					par_limit.second = b_facorg_lim * ctl_info->facparmax;
					limit_type_map[*name] = LimitType::FACT;
				}
				else if (b_facorg_lim < 0 && p_upgrade > b_facorg_lim / ctl_info->facparmax)
				{
					par_limit.first = true;
					par_limit.second = b_facorg_lim / ctl_info->facparmax;
					limit_type_map[*name] = LimitType::FACT;
				}
			}
			// Check parameter upper bound
			if ((!par_limit.first && p_upgrade > p_info->ubnd) ||
				(par_limit.first && par_limit.second > p_info->ubnd)) {
				par_limit.first = true;
				par_limit.second = p_info->ubnd;
				limit_type_map[*name] = LimitType::UBND;
			}
			// Check parameter lower bound
			else if ((!par_limit.first && p_upgrade < p_info->lbnd) ||
				(par_limit.first && par_limit.second < p_info->lbnd)) {
				par_limit.first = true;
				par_limit.second = p_info->lbnd;
				limit_type_map[*name] = LimitType::LBND;
			}
			// Add any limited parameters to model_parameters_at_limit
			if (par_limit.first) {
				active_ctl_parameters_at_limit.insert(*name, par_limit.second);
			}
		}
	}
}




Parameters SVDSolver::limit_parameters_freeze_all_ip(const Parameters &init_active_ctl_pars,
	Parameters &upgrade_active_ctl_pars, const Parameters &prev_frozen_active_ctl_pars)
{
	map<string, LimitType> limit_type_map;
	Parameters limited_ctl_parameters;
	const string *name;
	double p_init;
	double p_upgrade;
	double p_limit;
	Parameters new_frozen_active_ctl_parameters;
	pair<bool, double> par_limit;
	
	//remove frozen parameters
	upgrade_active_ctl_pars.erase(prev_frozen_active_ctl_pars);

	check_limits(init_active_ctl_pars, upgrade_active_ctl_pars, limit_type_map, limited_ctl_parameters);
	// Remove parameters at their upper and lower bound limits as these will be frozen
	vector<string> pars_at_bnds;
	for (auto ipar : limit_type_map)
	{
		if (ipar.second == LimitType::LBND || ipar.second == LimitType::UBND)
		{
			pars_at_bnds.push_back(ipar.first);
		}
	}
	limited_ctl_parameters.erase(pars_at_bnds);

	// Calculate most stringent limit factor on a PEST parameter
	double limit_factor = 1.0;
	double tmp_limit;
	string limit_parameter_name = "";
	Parameters init_numeric_pars = par_transform.active_ctl2numeric_cp(init_active_ctl_pars);
	Parameters upgrade_numeric_pars = par_transform.active_ctl2numeric_cp(upgrade_active_ctl_pars);
	Parameters numeric_parameters_at_limit = par_transform.active_ctl2numeric_cp(limited_ctl_parameters);
	for(auto &ipar : numeric_parameters_at_limit)
	{
		name = &(ipar.first);
		p_limit = ipar.second;
		p_init = init_numeric_pars.get_rec(*name);
		p_upgrade = upgrade_numeric_pars.get_rec(*name);
		tmp_limit = (p_limit - p_init) / (p_upgrade - p_init);
		if (tmp_limit < limit_factor)  {
			limit_factor = tmp_limit;
			limit_parameter_name = *name;
		}
	}
	// Apply limit factor to PEST upgrade parameters
	if (limit_factor != 1.0)
	{
		for(auto &ipar : upgrade_numeric_pars)
		{
			name = &(ipar.first);
			p_init = init_numeric_pars.get_rec(*name);
			ipar.second = p_init + (ipar.second - p_init) *  limit_factor;
		}
	}

	//Transform parameters back their ative control state and freeze any that violate their bounds
	upgrade_active_ctl_pars = par_transform.numeric2active_ctl_cp(upgrade_numeric_pars);


	check_limits(init_active_ctl_pars, upgrade_active_ctl_pars, limit_type_map, limited_ctl_parameters);
	for (auto &ipar : upgrade_active_ctl_pars)
	{
		name = &(ipar.first);
		if(limit_type_map[*name] == LimitType::UBND)
		{
			double limit_value = ctl_par_info_ptr->get_parameter_rec_ptr(*name)->ubnd;
			new_frozen_active_ctl_parameters[*name] = limit_value;
		}
		else if (limit_type_map[*name] == LimitType::LBND)
		{
			double limit_value = ctl_par_info_ptr->get_parameter_rec_ptr(*name)->lbnd;
			new_frozen_active_ctl_parameters[*name] = limit_value;
		}

	}

	// Impose frozen Parameters
	for (auto &ipar : prev_frozen_active_ctl_pars)
	{
		upgrade_active_ctl_pars[ipar.first] = ipar.second;
	}

	for (auto &ipar : new_frozen_active_ctl_parameters)
	{
		upgrade_active_ctl_pars[ipar.first] = ipar.second;
	}
	return new_frozen_active_ctl_parameters;
}

void SVDSolver::param_change_stats(double p_old, double p_new, bool &have_fac, double &fac_change, bool &have_rel, double &rel_change) 
{
	have_rel = have_fac = true;
	double a = max(abs(p_new), abs(p_old));
		double b = min(abs(p_new), abs(p_old));
		// compute relative change
		if (p_old == 0) {
			have_rel = false;
			rel_change = -9999;
		}
		else 
		{
			rel_change = (p_old - p_new) / p_old;
		}
		//compute factor change
		if (p_old == 0.0 || p_new == 0.0) {
			have_fac = false;
			fac_change = -9999;
		}
		else {
			fac_change = a / b;
		}
	}


void SVDSolver::iteration_update_and_report(ostream &os, ModelRun &upgrade, TerminationController &termination_ctl)
{
	const string *p_name;
	double p_old, p_new;
	double fac_change=-9999, rel_change=-9999;
	bool have_fac=false, have_rel=false;
	double max_fac_change = 0;
	double max_rel_change = 0;
	const string *max_fac_par = 0;
	const string *max_rel_par = 0;
	const Parameters &old_ctl_pars = cur_solution.get_ctl_pars();
	const Parameters &new_ctl_pars = upgrade.get_ctl_pars();

	os << "    Parameter Upgrades (Control File Parameters)" << endl;
	os << "      Parameter     Current       Previous       Factor       Relative" << endl;
	os << "        Name         Value         Value         Change        Change" << endl;
	os << "      ----------  ------------  ------------  ------------  ------------" << endl;

	for( const auto &ipar : new_ctl_pars)
	{
		p_name = &(ipar.first);
		p_new =  ipar.second;
		p_old = old_ctl_pars.get_rec(*p_name);
		param_change_stats(p_old, p_new, have_fac, fac_change, have_rel, rel_change);
		if (fac_change >= max_fac_change) 
		{
			max_fac_change = fac_change;
			max_fac_par = p_name;
		}
		if (abs(rel_change) >= abs(max_rel_change))
		{
			max_rel_change = rel_change;
			max_rel_par = p_name;
		}
		os << right;
		os << "    " << setw(12) << *p_name;
		os << right;
		os << "  " << setw(12) << p_new;
		os << "  " << setw(12) << p_old;
		if (have_fac)
			os << "  " << setw(12) << fac_change;
		else
			os << "  " << setw(12) << "N/A";
		if (have_rel)
			os << "  " << setw(12) << rel_change;
		else
			os << "  " << setw(12) << "N/A";
		os << endl;
	}
	os << "       Maximum changes in \"control file\" parameters:" << endl;
	os << "         Maximum relative change = " << max_rel_change << "   [" << *max_rel_par << "]" << endl;
	os << "         Maximum factor change = " << max_fac_change << "   [" << *max_fac_par << "]" << endl;
	os << endl;
	max_fac_change = 0;
	max_rel_change = 0;
	max_fac_par = 0;
	max_rel_par = 0;
	os << "    Parameter Upgrades (Transformed Numeric Parameters)" << endl;
	os << "      Parameter     Current       Previous       Factor       Relative" << endl;
	os << "        Name         Value         Value         Change        Change" << endl;
	os << "      ----------  ------------  ------------  ------------  ------------" << endl;

	const Parameters old_numeric_pars = par_transform.ctl2numeric_cp(cur_solution.get_ctl_pars());
	const Parameters new_numeric_pars = par_transform.ctl2numeric_cp(upgrade.get_ctl_pars());
	for( const auto &ipar : new_numeric_pars)
	{
		p_name = &(ipar.first);
		p_new =  ipar.second;
		p_old = old_numeric_pars.get_rec(*p_name);
		param_change_stats(p_old, p_new, have_fac, fac_change, have_rel, rel_change);
		if (fac_change >= max_fac_change) 
		{
			max_fac_change = fac_change;
			max_fac_par = p_name;
		}
		if (abs(rel_change) >= abs(max_rel_change))
		{
			max_rel_change = rel_change;
			max_rel_par = p_name;
		}
		os << right;
		os << "    " << setw(12) << *p_name;
		os << right;
		os << "  " << setw(12) << p_new;
		os << "  " << setw(12) << p_old;
		if (have_fac)
			os << "  " << setw(12) << fac_change;
		else
			os << "  " << setw(12) << "N/A";
		if (have_rel)
			os << "  " << setw(12) << rel_change;
		else
			os << "  " << setw(12) << "N/A";
		os << endl;
	}
	os << "       Maximum changes in \"transformed numeric\" parameters:" << endl;
	os << "         Maximum relative change = " << max_rel_change << "   [" << *max_rel_par << "]" << endl;
	os << "         Maximum factor change = " << max_fac_change << "   [" << *max_fac_par << "]" << endl;
	termination_ctl.process_iteration(upgrade.get_phi(), max_rel_change);
}

bool SVDSolver::par_heading_out_bnd(double p_org, double p_del, double lower_bnd, double upper_bnd)
{
	bool out_of_bnd = false;
	double tolerance = 1.0e-5;
	if (((1.0 + tolerance) * p_org > upper_bnd && p_del > 0) || ((1.0 - tolerance) * p_org < lower_bnd && p_del < 0))
	{
		out_of_bnd = true;
	}
	return out_of_bnd;
}

int SVDSolver::check_bnd_par(Parameters &new_freeze_active_ctl_pars, const Parameters &current_active_ctl_pars,
	const Parameters &del_upgrade_active_ctl_pars, const Parameters &del_grad_active_ctl_pars)
{
	int num_upgrade_out_grad_in = 0;
	double p_org;
	double p_del;
	double upper_bnd;
	double lower_bnd;
	const string *name_ptr;
	const auto it_end = del_upgrade_active_ctl_pars.end();
	for (const auto &ipar : current_active_ctl_pars)
	{
		name_ptr = &(ipar.first);
		const auto it = del_upgrade_active_ctl_pars.find(*name_ptr);

		if (it != it_end)
		{
			//first check upgrade parameters
			p_del = it->second;
			p_org = current_active_ctl_pars.get_rec(*name_ptr);
			upper_bnd = ctl_par_info_ptr->get_parameter_rec_ptr(*name_ptr)->ubnd;
			lower_bnd = ctl_par_info_ptr->get_parameter_rec_ptr(*name_ptr)->lbnd;
			//these are active parameters so this is not really necessary - just being extra safe
			bool par_active = ctl_par_info_ptr->get_parameter_rec_ptr(*name_ptr)->is_active();
			bool par_going_out = par_heading_out_bnd(p_org, p_del, lower_bnd, upper_bnd);
			//if gradient parameters are provided, also check these
			if (par_active &&  par_going_out && del_grad_active_ctl_pars.size() > 0)
			{
				const auto it_grad = del_grad_active_ctl_pars.find(*name_ptr);
				if (it_grad != del_grad_active_ctl_pars.end())
				{
					p_del = it_grad->second;
					par_going_out = par_heading_out_bnd(p_org, p_del, lower_bnd, upper_bnd);
				}
				else
				{
					++num_upgrade_out_grad_in;
				}
			}
			if (par_going_out)
				new_freeze_active_ctl_pars.insert(*name_ptr, p_org);

		}
	}
	return num_upgrade_out_grad_in;
}

void SVDSolver::limit_parameters_ip(const Parameters &init_active_ctl_pars, Parameters &upgrade_active_ctl_pars, 
										  LimitType &limit_type, const Parameters &frozen_active_ctl_pars, 
										  bool ignore_upper_lower)
{
	map<string, LimitType> limit_type_map;
	limit_type = LimitType::NONE;
	const string *name;
	double p_init;
	double p_upgrade;
	double p_limit;
	
	Parameters limited_active_ctl_parameters;
	//remove forozen parameters from upgrade pars
	upgrade_active_ctl_pars.erase(frozen_active_ctl_pars);

	check_limits(init_active_ctl_pars, upgrade_active_ctl_pars, limit_type_map, limited_active_ctl_parameters);

	//delete any limits cooresponding to ignored types
	for (auto it = limited_active_ctl_parameters.begin(); it != limited_active_ctl_parameters.end(); )
	{
		const string &name = (*it).first;
		const LimitType l_type = limit_type_map[name];

		auto temp_it = it;
		++temp_it;

		if (l_type == LimitType::LBND || l_type == LimitType::UBND)
		{
			limited_active_ctl_parameters.erase(it);
		}

		it = temp_it;
	}
	// Calculate most stringent limit factor on a numeric PEST parameters
	double limit_factor= 1.0;
	double tmp_limit;
	string limit_parameter_name = "";
	Parameters limited_numeric_parameters = par_transform.active_ctl2numeric_cp(limited_active_ctl_parameters);
	//this can be optimized to just compute init_numeric_parameters for those parameters at their limits
	Parameters init_numeric_pars = par_transform.active_ctl2numeric_cp(init_active_ctl_pars);
	Parameters upgrade_numeric_pars = par_transform.active_ctl2numeric_cp(upgrade_active_ctl_pars);
	for (auto &ipar : limited_numeric_parameters)
	{
		name = &(ipar.first);
		p_limit = ipar.second;
		p_init = init_numeric_pars.get_rec(*name);
		p_upgrade = upgrade_numeric_pars.get_rec(*name);
		tmp_limit = (p_limit - p_init) / (p_upgrade - p_init);
		if (tmp_limit < limit_factor)
		{
			limit_factor = tmp_limit;
			limit_parameter_name = *name;
			limit_type = limit_type_map[*name];
		}
	}
	// Apply limit factor to numeric PEST upgrade parameters
	if (limit_factor != 1.0)
	{
		for (auto &ipar : upgrade_numeric_pars)
		{
			name = &(ipar.first);
			p_init = init_numeric_pars.get_rec(*name);
			ipar.second = p_init + (ipar.second - p_init) *  limit_factor;
		}
	}
	//Convert newly limited parameters to their derivative state
	upgrade_active_ctl_pars = par_transform.numeric2active_ctl_cp(upgrade_numeric_pars);
	// Impose frozen Parameters as they were removed in the beginning
	for (auto &ipar : frozen_active_ctl_pars)
	{
		upgrade_active_ctl_pars[ipar.first] = ipar.second;
	}
}

void SVDSolver::dynamic_weight_adj(const Jacobian &jacobian, QSqrtMatrix &Q_sqrt,
	const Eigen::VectorXd &residuals_vec, const vector<string> &obs_names_vec,
	const Parameters &base_run_active_ctl_par, const Parameters &freeze_active_ctl_pars)
{
	//If running in regularization mode, adjust the regularization weights
	// define a function type for upgrade methods 
	Parameters new_pars;
	LimitType limit_type = LimitType::NONE;
	for (int i = 0; i < 6; ++i)
	{
		typedef void(SVDSolver::*UPGRADE_FUNCTION) (const Jacobian &jacobian, const QSqrtMatrix &Q_sqrt,
			const Eigen::VectorXd &Residuals, const vector<string> &obs_name_vec,
			const Parameters &base_ctl_pars, const Parameters &prev_frozen_ctl_pars,
			double lambda, Parameters &ctl_upgrade_pars, Parameters &upgrade_ctl_del_pars,
			Parameters &grad_ctl_del_pars, MarquardtMatrix marquardt_type);

		UPGRADE_FUNCTION calc_lambda_upgrade = &SVDSolver::calc_lambda_upgrade_vec_JtQJ;

		if (mat_inv == MAT_INV::Q12J)
		{
			calc_lambda_upgrade = &SVDSolver::calc_lambda_upgrade_vecQ12J;
		}
		// need to remove parameters frozen due to failed jacobian runs when calling calc_lambda_upgrade_vec
		//Freeze Parameters at the boundary whose ugrade vector and gradient both head out of bounds
		Parameters upgrade_ctl_del_pars;
		Parameters grad_ctl_del_pars;
		map<string, LimitType> limit_type_map;
		Parameters limited_ctl_parameters;

		(*this.*calc_lambda_upgrade)(jacobian, Q_sqrt, residuals_vec, obs_names_vec,
			base_run_active_ctl_par, freeze_active_ctl_pars, 0, new_pars, upgrade_ctl_del_pars,
			grad_ctl_del_pars, MarquardtMatrix::IDENT);
		limit_parameters_ip(base_run_active_ctl_par, new_pars,
			limit_type, freeze_active_ctl_pars, true);

		Parameters new_ctl_pars = par_transform.active_ctl2ctl_cp(new_pars);
		Parameters delta_par = par_transform.active_ctl2numeric_cp(new_pars)
			- par_transform.active_ctl2numeric_cp(base_run_active_ctl_par);
		vector<string> numeric_par_names = new_pars.get_keys();
		VectorXd delta_par_vec = transformable_2_egien_vec(delta_par, numeric_par_names);
		Eigen::SparseMatrix<double> jac = jacobian.get_matrix(obs_names_vec, numeric_par_names);
		VectorXd delta_obs_vec = jac * delta_par_vec;
		Transformable delta_obs(obs_names_vec, delta_obs_vec);
		Observations projected_obs = cur_solution.get_obs();
		projected_obs += delta_obs;

		double mu = Q_sqrt.get_tikhonov_weight();
		PhiComponets cur_phi_comp = cur_solution.get_obj_func_ptr()->get_phi_comp(cur_solution.get_obs(), cur_solution.get_ctl_pars());
		PhiComponets proj_phi_comp = cur_solution.get_obj_func_ptr()->get_phi_comp(projected_obs, new_ctl_pars);
		double target_phi = (cur_phi_comp.meas + cur_phi_comp.regul) * (1.0 - 0.3);
		double proj_phi = proj_phi_comp.meas + proj_phi_comp.regul*mu;
		double target_phi_regul = target_phi - proj_phi_comp.meas;
		if (proj_phi_comp.meas != 0)
		{
			double new_mu = target_phi_regul / proj_phi_comp.regul;
			Q_sqrt.set_tikhonov_weight(mu);
		}
	}
}
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
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <math.h>
#include <sstream>
#include <Eigen/Dense>
#include <cassert>
#include <iostream>
#include "Transformation.h"
#include "Transformable.h"
#include "SVD_PROPACK.h"
#include "eigen_tools.h"
#include "Jacobian.h"
#include "QSqrtMatrix.h"
#include "pest_data_structs.h"
using namespace std;
using namespace Eigen;

///////////////// Transformation Methods /////////////////


///////////////// TranMapBase Methods /////////////////
void TranMapBase::insert(const string &item_name, double item_value)
{
	items[item_name] = item_value;
}


void TranMapBase::insert(const Parameters &pars)
{
	for (const auto &ipar : pars)
	{
		items[ipar.first] = ipar.second;
	}
}

void TranMapBase::reset(const Parameters &pars)
{
	items.clear();
	for (const auto &ipar : pars)
	{
		items[ipar.first] = ipar.second;
	}
}

void TranMapBase::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranMapBase)" << endl; 
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  value = " << (*b).second << endl;   
	}
}

pair<bool, double> TranMapBase::get_value(const string &name) const
{
	pair<bool, double> ret_val(false, 0.0);
	map<string, double>::const_iterator it;
	
	it = items.find(name);
	if (it !=items.end()) {
		ret_val = pair<bool, double>(true, (*it).second);
	}
	return ret_val;
}

///////////////// TranSetBase Methods /////////////////
void TranSetBase::insert(const string &item_name)
{
	items.insert(item_name);
}

void TranSetBase::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranSetBase)" << endl;
	for (set<string>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << *b << endl;   
	}
}

bool TranSetBase::has_value(const string &name) const
{
	bool ret_val = false;
	set<string>::const_iterator it;
	
	it = items.find(name);
	if (it !=items.end()) {
		ret_val = true;
	}
	return ret_val;
}

///////////////// TranOffset Methods /////////////////
void TranOffset::forward(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second += (*b).second;
		}
	}
}

void TranOffset::reverse(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second -= b->second;
		}
	}
}

void TranOffset::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranOffset)" << endl; 
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  offset value = " << (*b).second << endl;   
	}
}

void TranOffset::d2_to_d1(Transformable &del_data, Transformable &data)
{
	// Offset transformation does not affect derivatives.
	// Nothing to do.
}

void TranOffset::d1_to_d2(Transformable &del_data, Transformable &data)
{
	// Offset transformation does not affect derivatives.
	// Nothing to do.
}


///////////////// TranScale Methods /////////////////
void TranScale::forward(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second *= b->second;
		}
	}
}


void TranScale::reverse(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second /= b->second;
		}
	}
}

void TranScale::d1_to_d2(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (map<string, double>::const_iterator b = items.begin(), e = items.end();
		b != e; ++b) {
		del_data_iter = del_data.find(b->first);
		if (del_data_iter != del_data_end)
		{
			(*del_data_iter).second *= b->second;
		}
	}
	forward(data);
}

void TranScale::d2_to_d1(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (map<string, double>::const_iterator b = items.begin(), e = items.end();
		b != e; ++b) {
		del_data_iter = del_data.find(b->first);
		if (del_data_iter != del_data_end)
		{
			(*del_data_iter).second /= b->second;
		}
	}
	reverse(data);
}

void TranScale::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranScale)" << endl; 
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  scale value = " << (*b).second << endl;   
	}
}

///////////////// TranLog10 Methods /////////////////
void TranLog10::forward(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (set<string>::const_iterator b=items.begin(), e=items.end(); b!=e; ++b) 
	{
		data_iter = data.find(*b);
		if (data_iter != data_end)
		{
			(*data_iter).second = log10((*data_iter).second);
		}
	}
}

void TranLog10::reverse(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (set<string>::const_iterator b=items.begin(), e=items.end(); b!=e; ++b)
	{
		data_iter = data.find(*b);
		if (data_iter != data_end)
		{
			(*data_iter).second = pow(10.0, (*data_iter).second);
		}
	}
}


void TranLog10::d1_to_d2(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (set<string>::const_iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data_iter = del_data.find(*b);
		if (del_data_iter != del_data_end)
		{
			double d1 = data.get_rec(*b);
			double factor = 1.0 / (d1 * log(10.0));
			(*del_data_iter).second *= factor;
		}
	}
	forward(data);
}

void TranLog10::d2_to_d1(Transformable &del_data, Transformable &data)
{
	// this transformation takes x -> y = log10(x)
	// the factors = dy/dx = d/dx(log10(x) =  1/(y * ln(10))
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (set<string>::const_iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data_iter = del_data.find(*b);
		if (del_data_iter != del_data_end)
		{
			double d2 = data.get_rec(*b);
			double factor = pow(d2, 10.0) * log(10.0);
			(*del_data_iter).second *= factor;
		}
	}
	reverse(data);
}

void TranLog10::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranLog10)" << endl;
	for (set<string>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << *b << endl;   
	}
}

///////////////// TranInvLog10 Methods /////////////////
void TranInvLog10::forward(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (set<string>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(*b);
		if (data_iter != data_end)
		{
			(*data_iter).second = pow(10.0,(*data_iter).second);
		}
	}
}

void TranInvLog10::reverse(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (set<string>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(*b);
		if (data_iter != data_end)
		{
			(*data_iter).second = log10((*data_iter).second);
		}
	}
}


void TranInvLog10::d2_to_d1(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (set<string>::const_iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data_iter = del_data.find(*b);
		if (del_data_iter != del_data_end)
		{
			double d1 = data.get_rec(*b);
			double factor = 1.0 / (d1 * log(10.0));
			(*del_data_iter).second *= factor;
		}
	}
	reverse(data);
}

void TranInvLog10::d1_to_d2(Transformable &del_data, Transformable &data)
{
	// this transformation takes x -> y = log10(x)
	// the factors = dy/dx = d/dx(log10(x) =  1/(y * ln(10))
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (set<string>::const_iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data_iter = data.find(*b);
		if (del_data_iter != del_data_end)
		{
			double d2 = data.get_rec(*b);
			double factor = pow(d2, 10.0) * log(10.0);
			(*del_data_iter).second *= factor;
		}
	}
	forward(data);
}

void TranInvLog10::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranInvLog10)" << endl;
	for (set<string>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << *b << endl;   
	}
}


///////////////// TranFixed Methods /////////////////
void TranFixed::forward(Transformable &data)
{
	for (map<string,double>::iterator b=items.begin(), e=items.end(); b!=e; ++b)
	{
		data.erase(b->first);
	}
}

void TranFixed::reverse(Transformable &data)
{
	for (map<string,double>::iterator b=items.begin(), e=items.end(); b!=e; ++b)
	{
		data.insert(b->first, b->second);
	}
}

void TranFixed::d1_to_d2(Transformable &del_data, Transformable &data)
{
	for (map<string, double>::iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data.erase(b->first);
	}
	forward(data);
}

void TranFixed::d2_to_d1(Transformable &del_data, Transformable &data)
{
	for (map<string, double>::iterator b = items.begin(), e = items.end(); b != e; ++b)
	{
		del_data.insert(b->first, 0.0);
	}
	reverse(data);
}


void TranFixed::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranFixed)" << endl; 
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  imposed value = " << (*b).second << endl;   
	}
}

void TranFrozen::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranFrozen)" << endl; 
	for (map<string,double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  imposed value = " << (*b).second << endl;   
	}
}

void TranTied::insert(const string &item_name, const pair<string, double> &item_value)
{
	items[item_name] = item_value;
}


void TranTied::d1_to_d2(Transformable &del_data, Transformable &data)
{
	for (map<string, pair_string_double>::iterator ii = items.begin(); ii != items.end(); ++ii)
	{
		del_data.erase(ii->first);
	}
	forward(data);
}

void TranTied::d2_to_d1(Transformable &del_data, Transformable &data)
{
	string const *base_name;
	double *factor;
	Transformable::iterator base_iter;
	for (map<string, pair_string_double>::iterator b = items.begin(), e = items.end();
		b != e; ++b)
	{
		base_name = &(b->second.first);
		factor = &(b->second.second);
		base_iter = data.find(*base_name);
		if (base_iter != data.end())
		{
			del_data.insert(b->first, (*base_iter).second * (*factor));
		}
	}
	reverse(data);
}

void TranTied::reverse(Transformable &data)
{
	string const *base_name;
	double *factor;
	Transformable::iterator base_iter;
	for (map<string, pair_string_double>::iterator b=items.begin(), e=items.end();
		b!=e; ++b)
	{
		base_name = &(b->second.first);
		factor = &(b->second.second);
		base_iter = data.find(*base_name);
		if (base_iter != data.end())
		{
			data.insert(b->first, (*base_iter).second * (*factor));
		}
	}
}

void TranTied::forward(Transformable &data)
{
	for (map<string, pair_string_double>::iterator ii=items.begin(); ii!=items.end(); ++ii)
	{
		data.erase(ii->first);
	}
}


void TranTied::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranTied)" << endl; 
	for (map<string, pair_string_double>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << "   tied to \"" << (*b).second.first << 
				"\" with factor " <<  (*b).second.second<<  endl;   
	}
}


const  Eigen::SparseMatrix<double>& TranSVD::get_vt() const
{
	return Vt;
}


TranSVD::TranSVD(int _max_sing, double _eign_thresh, const string &_name) : Transformation(_name)
{
	tran_svd_pack = new SVD_EIGEN(_max_sing, _eign_thresh);
}

void TranSVD::set_SVD_pack_propack()
{
	int max_sing = tran_svd_pack->get_max_sing();
	double eigthresh = tran_svd_pack->get_eign_thres();
	delete tran_svd_pack;
	tran_svd_pack = new SVD_PROPACK(max_sing, eigthresh);
}

void TranSVD::calc_svd()
{
	stringstream sup_name;
	VectorXd Sigma_trunc;
	tran_svd_pack->solve_ip(SqrtQ_J, Sigma, U, Vt, Sigma_trunc);
	// calculate the number of singluar values above the threshold

	int n_sing_val = Sigma.size();

	super_parameter_names.clear();
	for(int i=0; i<n_sing_val; ++i) {
		sup_name.str("");
		sup_name << "sup_";
		sup_name << i+1;
		super_parameter_names.push_back(sup_name.str());
	}
	if (n_sing_val <= 0 )
	{
		throw PestError("TranSVD::update() - super parameter transformation returned 0 super parameters.  Jacobian must equal 0.");
	}
}

void TranSVD::update_reset_frozen_pars(const Jacobian &jacobian, const QSqrtMatrix &Q_sqrt, const Parameters &base_numeric_pars,
		int maxsing, double _eigthresh, const vector<string> &par_names, const vector<string> &_obs_names,
		const Parameters &_frozen_derivative_pars)
{
	stringstream sup_name;
	super_parameter_names.clear();

	tran_svd_pack->set_max_sing(maxsing);
	tran_svd_pack->set_eign_thres(_eigthresh);
	obs_names = _obs_names;

	

	//these are where the derivative was computed so they can be different than the frozen values;
	init_base_numeric_parameters = base_numeric_pars;  
	base_parameter_names = par_names;
	//remove frozen parameters from base_parameter_names
	auto end_iter = std::remove_if(base_parameter_names.begin(), base_parameter_names.end(),
		[&_frozen_derivative_pars](string &str)->bool{return _frozen_derivative_pars.find(str)!=_frozen_derivative_pars.end();});
	base_parameter_names.resize(std::distance(base_parameter_names.begin(), end_iter));
	frozen_derivative_parameters = _frozen_derivative_pars;
	//remove frozen derivatives from matrix parameter list
	std::remove_if(base_parameter_names.begin(), base_parameter_names.end(),
		[this](string &str)->bool{return this->frozen_derivative_parameters.find(str)!=this->frozen_derivative_parameters.end();});

	SqrtQ_J = Q_sqrt.get_sparse_matrix(obs_names) * jacobian.get_matrix(obs_names, base_parameter_names);

	calc_svd();
}

void TranSVD::update_add_frozen_pars(const Parameters &frozen_pars)
{
	vector<int> del_col_ids;
	Parameters new_frozen_pars;
	for (auto &ipar : frozen_pars)
	{
		auto iter = frozen_derivative_parameters.find(ipar.first);
		if (iter == frozen_derivative_parameters.end())
		{
			new_frozen_pars.insert(ipar);
		}
	}

	frozen_derivative_parameters.insert(new_frozen_pars);

	// build list of columns that needs to be removed from the matrix
	for (int i=0; i<base_parameter_names.size(); ++i)
	{
		if(new_frozen_pars.find(base_parameter_names[i]) != new_frozen_pars.end())
		{
			del_col_ids.push_back(i);
		}
	}

	if (del_col_ids.size() == base_parameter_names.size())
	{
		throw PestError("TranSVD::update_add_frozen_pars - All parameters are frozen in SVD transformation");
	}
	//remove frozen parameters from base_parameter_names
	auto end_iter = std::remove_if(base_parameter_names.begin(), base_parameter_names.end(),
		[&new_frozen_pars](string &str)->bool{return new_frozen_pars.find(str)!=new_frozen_pars.end();});
	base_parameter_names.resize(std::distance(base_parameter_names.begin(), end_iter));
	matrix_del_cols(SqrtQ_J, del_col_ids);
	calc_svd();
}
void TranSVD::reverse(Transformable &data)
{
	// Transform super-parameters to base parameters
	assert(Vt.cols() == base_parameter_names.size());
	int n_base = Vt.cols();
	vector<double> super_par_vec = data.get_data_vec(super_parameter_names);
	vector<double>::iterator it;
	for (it=super_par_vec.begin(); it!=super_par_vec.end(); ++it)
	{
		(*it) -= 10.0;
	}
	Transformable ret_base_pars;
	int n_sing_val = Sigma.size();
	VectorXd delta_base_mat = Vt.block(0,0,n_sing_val, Vt.cols()).transpose() *  stlvec_2_egienvec(super_par_vec);
	for (int i=0; i<n_base; ++i) {
		ret_base_pars.insert(base_parameter_names[i], delta_base_mat(i) + init_base_numeric_parameters.get_rec(base_parameter_names[i]));
	}

	data = ret_base_pars;
}

void TranSVD::forward(Transformable &data)
{
	//Transform base parameters to super-parameters
	Transformable super_pars;
	VectorXd value;

	Transformable delta_data = init_base_numeric_parameters;
	delta_data *= 0.0;

	for (auto &it : data)
	{
		delta_data[it.first] = it.second - init_base_numeric_parameters.get_rec(it.first);
	}
	int n_sing_val = Sigma.size();
	value = Vt * delta_data.get_data_eigen_vec(base_parameter_names);
	for (int i=0; i<n_sing_val; ++i) {
		super_pars.insert(super_parameter_names[i], value(i)+10.0);
	}
	data = super_pars;
}


void TranSVD::d1_to_d2(Transformable &del_data, Transformable &data)
{
	Transformable new_data;
	VectorXd d1_vec = del_data.get_partial_data_eigen_vec(base_parameter_names);
	VectorXd d2_vec = Vt * d1_vec;
	for (int i = 0; i<Sigma.size(); ++i) {
		new_data[super_parameter_names[i]] = d2_vec[i];
	}
	new_data = del_data;
	forward(data);
}


void TranSVD::d2_to_d1(Transformable &del_data, Transformable &data)
{
	Transformable new_data;
	VectorXd d2_vec = del_data.get_partial_data_eigen_vec(super_parameter_names);
	VectorXd d1_vec = Vt.transpose() * d2_vec;
	for (int i = 0; i < base_parameter_names.size(); ++i)
	{
		new_data[base_parameter_names[i]] = d1_vec[i];
	}
	for (auto & ipar : frozen_derivative_parameters)
	{
		new_data[ipar.first] = 0;
	}
	del_data = new_data;
	reverse(data);
}


ParameterGroupInfo TranSVD::build_par_group_info(const ParameterGroupInfo &base_pg_info)
{
	double derinc_sup;
	double derinc_par;
	int max_col;
	double max_val;
	ParameterGroupInfo pg_info;
	stringstream grp_name;
	for (int i_sup=0, n_sup=super_parameter_names.size(); i_sup < n_sup; ++i_sup)
	{
		get_MatrixXd_row_abs_max(Vt, i_sup, &max_col, &max_val);
		derinc_par = base_pg_info.get_group_rec_ptr(base_parameter_names[max_col])->derinc;
		derinc_sup = .01;
		grp_name.str("");
		grp_name << "g_" << super_parameter_names[i_sup];
		ParameterGroupRec sup_rec(grp_name.str(), "ABSOLUTE", derinc_sup, 0.0, "SWITCH", 2.0, "PARABOLIC");
		//add new group
		pg_info.insert_group(grp_name.str(), sup_rec);
		// connect super parameter to new group
		pg_info.insert_parameter_link(super_parameter_names[i_sup], grp_name.str());
	}
	return pg_info;
}


Parameters TranSVD::map_basepar_to_super(const Parameters &base_pars)
{
	Parameters super_pars;
	VectorXd base_par_vec = base_pars.get_partial_data_eigen_vec(base_parameter_names);
	VectorXd super_par_vec = Vt * base_par_vec;
	for (int i=0; i<super_parameter_names.size(); ++i)
	{
		super_pars[super_parameter_names[i]] = super_par_vec(i);
	}
	return super_pars;
}

void TranSVD::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranSVD)" << endl; 
	os << "  Singular Values = " << Sigma << endl;
}

void TranNormalize::forward(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,NormData>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second += b->second.offset;
			(*data_iter).second *= b->second.scale;
		}
	}
}


void TranNormalize::reverse(Transformable &data)
{
	Transformable::iterator data_iter, data_end = data.end();
	for (map<string,NormData>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
		data_iter = data.find(b->first);
		if (data_iter != data_end)
		{
			(*data_iter).second /= b->second.scale;
			(*data_iter).second -= b->second.offset;
		}
	}
}



void TranNormalize::d1_to_d2(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (map<string, NormData>::const_iterator b = items.begin(), e = items.end();
		b != e; ++b) {
		del_data_iter = data.find(b->first);
		if (del_data_iter != del_data_end)
		{
			(*del_data_iter).second *= b->second.scale;
		}
	}
	forward(data);
}

void TranNormalize::d2_to_d1(Transformable &del_data, Transformable &data)
{
	Transformable::iterator del_data_iter, del_data_end = del_data.end();
	for (map<string, NormData>::const_iterator b = items.begin(), e = items.end();
		b != e; ++b) {
		del_data_iter = del_data.find(b->first);
		if (del_data_iter != del_data_end)
		{
			(*del_data_iter).second /= b->second.scale;
		}
	}
	reverse(data);
}

void TranNormalize::insert(const string &item_name, double _offset, double _scale)
{
	items[item_name] = NormData(_offset, _scale);
}

void TranNormalize::print(ostream &os) const
{
	os << "Transformation name = " << name << "; (type=TranNormalize)" << endl; 
	for (map<string,NormData>::const_iterator b=items.begin(), e=items.end();
		b!=e; ++b) {
			os << "  item name = " << (*b).first << ";  scale value = " << (*b).second.scale
				<< ";  offset value = " << (*b).second.offset <<endl;   
	}
}

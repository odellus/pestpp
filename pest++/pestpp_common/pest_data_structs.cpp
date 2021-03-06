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
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <list>
#include "pest_data_structs.h"
#include "utilities.h"
#include "pest_error.h"
#include "ParamTransformSeq.h"
#include "Transformation.h"
#include "pest_error.h"
#include "Transformable.h"

using namespace::std;
using namespace::pest_utils;


ostream& operator<< (ostream &os, const ControlInfo& val)
{
	os << "PEST Control Information" << endl;
	os << "    relparmax = " << val.relparmax << endl;
	os << "    facparmax = " << val.facparmax << endl;
	os << "    facorig = " << val.facorig << endl;
	os << "    phiredswh = " << val.phiredswh << endl;
	os << "    noptmax = " << val.noptmax << endl;
	os << "    phiredstp = " << val.phiredstp << endl;
	os << "    nphistp = " << val.nphistp << endl;
	os << "    nphinored = " << val.nphinored << endl;
	os << "    relparstp = " << val.relparstp << endl;
	os << "    nrelpar = " << val.nrelpar << endl;
	return os;
}

//////////////////  ParameterGroupRec Methods ////////////////////////////////

ParameterGroupRec& ParameterGroupRec::operator=(const ParameterGroupRec &rhs)
{
	name = rhs.name;
	inctyp = rhs.inctyp;
	derinc = rhs.derinc;
	derinclb = rhs.derinclb;
	forcen = rhs.forcen;
	derincmul = rhs.derincmul;
	dermthd = rhs.dermthd;
	return *this;
}

ostream& operator<< (ostream &os, const ParameterGroupRec& val)
{
	os << "PEST Parameter Group Information" << endl;
	os << "    name   = " << val.name << endl;
	os << "    inctyp = " << val.inctyp << endl;
	os << "    derinc = " << val.derinc << endl;
	os << "    derinclb = " << val.derinclb << endl;
	os << "    forcen = " << val.forcen << endl;
	os << "    derincmul = " << val.derincmul << endl;
	return os;
}

/////////////////////////////////////  ParameterGroupInfo Methods ///////////////////////

const ParameterGroupRec* ParameterGroupInfo::get_group_rec_ptr(const string &name) const
{
	const ParameterGroupRec *ret_val = 0;
	unordered_map<string, ParameterGroupRec*>::const_iterator g_iter;

	g_iter = parameter2group.find(name);
	if(g_iter != parameter2group.end()) {
		ret_val = (*g_iter).second;
	}
	return ret_val;
}


string ParameterGroupInfo::get_group_name(const string &par_name) const
{
	return get_group_rec_ptr(par_name)->name;
}

void ParameterGroupInfo::insert_group(const string &group_name, ParameterGroupRec &rec) 
{
	groups[group_name] = new ParameterGroupRec(rec);
}

void ParameterGroupInfo::insert_parameter_link(const string &parameter_name, const string & group_name)
{
	unordered_map<string, ParameterGroupRec*>::const_iterator g_iter;

	g_iter = groups.find(group_name);
	if(g_iter == groups.end()) {
		throw PestIndexError(group_name, "Invalid parameter group name");
	}
	else {
		parameter2group[parameter_name] = (*g_iter).second;
	}

}

const ParameterGroupInfo& ParameterGroupInfo::operator=(const ParameterGroupInfo &rhs)
{
	unordered_map<ParameterGroupRec*, ParameterGroupRec*> old2new;
	unordered_map<string, ParameterGroupRec*>::const_iterator it(rhs.groups.begin());
	unordered_map<string, ParameterGroupRec*>::const_iterator end(rhs.groups.end());
	for (; it != end; ++it) {
		ParameterGroupRec* new_ptr = new ParameterGroupRec(*(*it).second);
		groups[(*it).first] = new_ptr;
		old2new[(*it).second] = new_ptr;
	}
	unordered_map<ParameterGroupRec*, ParameterGroupRec*>::iterator it_find;
	it = rhs.parameter2group.begin();
	end =  rhs.parameter2group.end();
	for (; it != end; ++it) {
		it_find = old2new.find((*it).second);
		parameter2group[(*it).first] = (*it_find).second;
	}
	return *this;
}

ParameterGroupInfo::~ParameterGroupInfo()
{
	unordered_map<string, ParameterGroupRec*>::iterator it(groups.begin());
	unordered_map<string, ParameterGroupRec*>::iterator end(groups.end());
	for (; it != end; ++it) {
		delete (*it).second;
	}
}

ostream& operator<< (ostream &os, const ParameterGroupInfo &val)
{
	unordered_map<string, ParameterGroupRec*>::const_iterator it(val.parameter2group.begin());
	unordered_map<string, ParameterGroupRec*>::const_iterator end(val.parameter2group.end());
	for (; it != end; ++it) {
		os << *(it->second) << endl;
	}
	return os;
}


/////////////////////////////////////  ParameterRec Methods ///////////////////////

ostream& operator<< (ostream &os, const ParameterRec& val)
{
	os << "PEST Parameter Information" << endl;
	os << "    chglim = " << val.chglim << endl;
	os << "    lbnd = " << val.lbnd << endl;
	os << "    ubnd = " << val.ubnd << endl;
	os << "    group = " << val.group << endl;
	os << "    dercom = " << val.dercom << endl;
	return os;
}

/////////////////////////////////////  CtlParameterInfo Methods ///////////////////////



const ParameterRec* ParameterInfo::get_parameter_rec_ptr(const string &name) const
{
	const ParameterRec *ret_val = 0;
	unordered_map<string, ParameterRec>::const_iterator p_iter;

	p_iter = parameter_info.find(name);
	if(p_iter != parameter_info.end()) {
		ret_val = &((*p_iter).second);
	}
	return ret_val;
}

Parameters ParameterInfo::get_low_bnd(const vector<string> &keys) const
{
	Parameters l_bnd;
	vector<string>::const_iterator iend=keys.end();
	ParameterRec const *v_ptr;
	for (vector<string>::const_iterator i=keys.begin(); i!=iend; ++i)
	{
		v_ptr = get_parameter_rec_ptr(*i);
		if (v_ptr) {
			l_bnd.insert(*i, v_ptr->lbnd);
		}
		else {
			l_bnd.insert(*i, Parameters::no_data);
		}
	}
	return l_bnd;
}

Parameters ParameterInfo::get_up_bnd(const vector<string> &keys) const
{
	Parameters u_bnd;
	vector<string>::const_iterator iend=keys.end();
	ParameterRec const *v_ptr;
	for (vector<string>::const_iterator i=keys.begin(); i!=iend; ++i)
	{
		v_ptr = get_parameter_rec_ptr(*i);
		if (v_ptr) {
			u_bnd.insert(*i, v_ptr->ubnd);
		}
		else {
			u_bnd.insert(*i, Parameters::no_data);
		}
	}
	return u_bnd;
}

Parameters ParameterInfo::get_init_value(const vector<string> &keys) const
{
	Parameters init_value;
	vector<string>::const_iterator iend=keys.end();
	ParameterRec const *v_ptr;
	for (vector<string>::const_iterator i=keys.begin(); i!=iend; ++i)
	{
		v_ptr = get_parameter_rec_ptr(*i);
		if (!v_ptr) {
			init_value.insert(*i, v_ptr->ubnd);
		}
		else {
			init_value.insert(*i, Parameters::no_data);
		}
	}
	return init_value;
}


PestppOptions::PestppOptions(int _n_iter_base, int _n_iter_super, int _max_n_super, double _super_eigthres,
	SVD_PACK _svd_pack, MAT_INV _mat_inv, double _auto_norm, double _super_relparmax, int _max_run_fail)
	: n_iter_base(_n_iter_base), n_iter_super(_n_iter_super), max_n_super(_max_n_super), super_eigthres(_super_eigthres), 
	svd_pack(_svd_pack), mat_inv(_mat_inv), auto_norm(_auto_norm), super_relparmax(_super_relparmax),
	max_run_fail(_max_run_fail)
{
}

void PestppOptions::parce_line(const string &line)
{
	string key;
	string value;
	size_t found = line.find_first_of("#");
	if (found == string::npos) {
		found = line.length();
	}
	string tmp_line = line.substr(0, found);
	strip_ip(tmp_line, "both", "\t\n\r+ ");
	upper_ip(tmp_line);
	list<string> tokens;
	tokenize(tmp_line, tokens, "\t\n\r() ");
	while(tokens.size() >=2)
	{
		key = tokens.front();
		tokens.pop_front();
		value = tokens.front();
		tokens.pop_front();
		if (key=="MAX_N_SUPER"){
			convert_ip(value, max_n_super); 
		}
		else if (key=="SUPER_EIGTHRES"){
			convert_ip(value, super_eigthres); 
		}
		else if (key=="N_ITER_BASE"){
			convert_ip(value, n_iter_base); 
		}
		else if (key=="N_ITER_SUPER"){
			convert_ip(value, n_iter_super); 
		}
		else if (key=="SVD_PACK"){
			if(value == "PROPACK") svd_pack = PROPACK;
		}
		else if (key=="AUTO_NORM"){
			convert_ip(value, auto_norm); 
		}
		else if (key == "SUPER_RELPARMAX"){
			convert_ip(value, super_relparmax);
		}
		else if (key == "MAT_INV"){
			if (value == "Q1/2J") mat_inv = Q12J;
		}
		else if (key == "MAX_RUN_FAIL"){
			convert_ip(value, max_run_fail);
		}
		else {
			throw PestParsingError(line, "Invalid key word \"" + key +"\"");
		}
	}
}


ostream& operator<< (ostream &os, const ParameterInfo& val)
{
	for(unordered_map<string, ParameterRec>::const_iterator s=val.parameter_info.begin(),
				e=val.parameter_info.end(); s!=e; ++s) {
			os << (*s).second;
		}
	return os;
}




ostream& operator<< (ostream &os, const ObservationGroupRec& val)
{
	os << "    gtarg = " << val.gtarg << endl;
	os << "    covfile = " << val.covfile << endl;
	return os;
}

ostream& operator<< (ostream &os, const ObservationRec& val)
{
	os << "    weight = " << val.weight << endl;
	os << "    group = " << val.group << endl;
	return os;
}

bool ObservationGroupRec::is_regularization(const string &grp_name)
{
	bool is_reg = false;
	int found = upper_cp(grp_name).find("REGUL");
	if (found == 0) is_reg = true;
	return is_reg;

}

const ObservationRec* ObservationInfo::get_observation_rec_ptr(const string &name) const
{
	const ObservationRec *ret_val = 0;
	unordered_map<string, ObservationRec>::const_iterator p_iter;

	p_iter = observations.find(name);
	if(p_iter != observations.end()) {
		ret_val = &((*p_iter).second);
	}
	return ret_val;
}

const ObservationGroupRec* ObservationInfo::get_group_rec_ptr(const string &name) const
{
	const ObservationGroupRec *ret_val = 0;
	unordered_map<string, ObservationGroupRec>::const_iterator p_iter;

	p_iter = groups.find(name);
	if(p_iter != groups.end()) {
		ret_val = &((*p_iter).second);
	}
	return ret_val;
}


bool ObservationRec::is_regularization() const
{
	bool is_reg = false;
	int found = upper_cp(group).find("REGUL");
	if (found == 0) is_reg=true;
	return is_reg;
}

double ObservationInfo::get_weight(const string &obs_name) const
{
	return observations.find(obs_name)->second.weight;
}

string ObservationInfo::get_group(const string &obs_name) const
{
	return observations.find(obs_name)->second.group;
}

bool ObservationInfo::is_regularization(const string &obs_name) const
{
	return observations.find(obs_name)->second.is_regularization();
}

Observations ObservationInfo::get_regulatization_obs(const Observations &obs_in)
{
	Observations reg_obs;

	for (Observations::const_iterator b=obs_in.begin(), e=obs_in.end();
			b!=e; ++b)
	{
		if(is_regularization((*b).first)) {
			reg_obs.insert(*b);
		}
	}
	return reg_obs;
}


ostream& operator<< (ostream &os, const ObservationInfo& val)
{
	os << "PEST Observation Information" << endl;
	os << "PEST Observation Groups" << endl;
	for(unordered_map<string, ObservationGroupRec>::const_iterator b=val.groups.begin(),
		e=val.groups.end(); b!=e; ++b) {
			os << "    name = " << (*b).first << endl;
	}
	os << "PEST Observation Information" << endl;
	for(unordered_map<string, ObservationRec>::const_iterator b=val.observations.begin(),
		e=val.observations.end(); b!=e; ++b) {
			os << "  Observation Name = " << (*b).first << endl;
			os << (*b).second;
	}
	return os;
}

ostream& operator<< (ostream &os, const SVDInfo& val)
{
	os << "PEST SVD Information" << endl;
	os << "    maxsing = " << val.maxsing << endl;
	os << "    eigthresh = " << val.eigthresh << endl;
	return os;
}

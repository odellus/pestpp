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
#include "TerminationController.h"
#include <vector>
#include <algorithm>
#include "utilities.h"
using namespace pest_utils;

using namespace std;

TerminationController::TerminationController(int _noptmax, double _phiredstp,
	int _nphistp, int _nphinoger, double _relparstp, int _nrelpar) 
	: noptmax(_noptmax), phiredstp(_phiredstp), nphistp(_nphistp),
	nphinoger(_nphinoger), relparstp(_relparstp), 
	nrelpar(_nrelpar), nphinoger_count(0), nrelpar_count(0), nopt_count(0), 
	terminate_code(false)
{
}

void TerminationController::reset() {
	nopt_count = 0;
	nphinoger_count = 0;
	lowest_phi.clear();
	terminate_code = false;
}

bool TerminationController::process_iteration(double phi, double relpar)
{
	++nopt_count;
	// keep track of number of iterations since lowest phi
	if (lowest_phi.size() == 0 || phi <= lowest_phi.front()) {
		nphinoger_count = 0;
	}
	else {
		++nphinoger_count;
	}

	// keep track of NPHISTP lowest phi's
	if (lowest_phi.size() < nphistp) {
		lowest_phi.push_back(phi);
	}
	else if (phi < lowest_phi.back()) {
		lowest_phi.back() = phi;
	}
	sort(lowest_phi.begin(), lowest_phi.end());

	// Check maximum relative parameter change
	if (relpar >= relparstp) {
		++nrelpar_count;
	}
	else {
		nrelpar_count = 0;
	}
	return check_last_iteration();
}
bool TerminationController::check_last_iteration()
{
	double min_phi_diff = lowest_phi.back() - lowest_phi.front();
	// Impose Termination Criteria
	if (nphinoger_count > nphinoger)
		terminate_code = true;
	else if (lowest_phi.size() >= nphistp && min_phi_diff <= phiredstp)
		terminate_code = true;
	else if (nrelpar_count > nrelpar)
		terminate_code = true;
	else if (nopt_count >= noptmax)
		terminate_code = true;
	else 
		terminate_code = false;
	return terminate_code;
}

const TerminationController& TerminationController::operator=(const TerminationController &rhs)
{
	noptmax = rhs.noptmax;
	nphinoger = rhs.nphinoger;
	nphinoger_count = rhs.nphinoger_count;
	nrelpar = rhs.nrelpar;
	nrelpar_count = rhs.nrelpar_count;
	nphistp = rhs.nphistp;
	phiredstp = rhs.phiredstp;
	relparstp = rhs.relparstp;
	lowest_phi = rhs.lowest_phi;
	return *this;
}


void TerminationController::save_state(std::ostream &fout)
{

	fout << "termination_info_1 " << noptmax << " " << nopt_count << " " << nphinoger << " " 
		<< nphinoger_count << " " << nrelpar << endl;
	fout << "termination_info_2 " << nrelpar_count << " " << nphistp << " " << phiredstp << " " << relparstp << endl;
	fout << "termination_info_3 ";
	for (double &a : lowest_phi)
	{
		fout << " " << a;
	}
	fout << endl;
}



void  TerminationController::read_state(const std::string &line)
{
	vector<string> tokens;

	tokenize(line, tokens);
	try
	{
		string line_type = upper_cp(tokens[0]);
		if (line_type == "TERMINATION_INFO_1")
		{
			convert_ip(tokens[1], noptmax);
			convert_ip(tokens[2], nopt_count);
			convert_ip(tokens[3], nphinoger);
			convert_ip(tokens[4], nphinoger_count);
			convert_ip(tokens[5], nrelpar);
		}
		else if (line_type == "TERMINATION_INFO_2")
		{
			convert_ip(tokens[1], nrelpar);
			convert_ip(tokens[2],nphistp);
			convert_ip(tokens[3],phiredstp);
			convert_ip(tokens[4],relparstp );
		}
		else if (line_type == "TERMINATION_INFO_3")
		{
			for (int i=1; i<tokens.size(); ++i)
			{
				double val = convert_cp<double>(tokens[1]);
				lowest_phi.push_back(val);
			}
		}
	}
	catch(std::runtime_error &e)
	{
		cerr << "Error processing restart file on line:" << endl;
		cerr << line << endl << endl;
		cerr << e.what() << endl << endl;
		throw(e);
	}
}

TerminationController::~TerminationController(void)
{
}

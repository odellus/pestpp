#include "YamrSlave.h"
#include "utilities.h"
#include "Serialization.h"
#include "system_variables.h"
#include <cassert>
#include <cstring>
#include <algorithm>
#include "system_variables.h"
#include "iopp.h"

using namespace pest_utils;

int  linpack_wrap(void);

extern "C"
{

	void wrttpl_(int *,
		char *,
		char *,
		int *,
		char *,
		double *,
		int *);

	void readins_(int *,
		char *,
		char *,
		int *,
		char *,
		double *,
		int *);
}



void YAMRSlave::init_network(const string &host, const string &port)
{
	w_init();


	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof hints);
	//Use this for IPv4 aand IPv6
	//hints.ai_family = AF_UNSPEC;
	//Use this just for IPv4;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = w_getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo);
	w_print_servinfo(servinfo, cout);
	cout << endl;
	// connect
	addrinfo* connect_addr = nullptr;
	while  (connect_addr == nullptr)
	{
		connect_addr = w_connect_first_avl(servinfo, sockfd);
		if (connect_addr == nullptr) {
			cerr << endl;
			cerr << "failed to connect to master" << endl;
		}
	}
	cout << "connection to master succeeded on socket: " << w_get_addrinfo_string(connect_addr) << endl << endl;
	freeaddrinfo(servinfo);

	fdmax = sockfd;
	FD_ZERO(&master);
	FD_SET(sockfd, &master);
	// send run directory to master
}


YAMRSlave::~YAMRSlave()
{
	w_close(sockfd);
	w_cleanup();
}

int YAMRSlave::recv_message(NetPackage &net_pack)
{
	fd_set read_fds;
	int err = -1;
	for(;;) {
	read_fds = master; // copy master
		if (w_select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			exit(4);
		}
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // got message to read
				if(( err=net_pack.recv(i)) <=0) // error or lost connection
				{
					vector<string> sock_name = w_getnameinfo_vec(i);
					if (err < 0) {
							cerr << "receive from master failed: " << sock_name[0] <<":" <<sock_name[1] << endl;
					}
					else {
						cerr << "lost connection to master: " << sock_name[0] <<":" <<sock_name[1] << endl;
						w_close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					}
				}
				else  
				{
					// received data sored in net_pack return to calling routine to process it
				}
			return err;
			}
		}
	}
	return err;
}


int YAMRSlave::send_message(NetPackage &net_pack, const void *data, unsigned long data_len)
{
	int err;
	int n;

	for (err = -1, n=0; err==-1; ++n)
	{
		err = net_pack.send(sockfd, data, data_len);
	}
	return err;
}

string YAMRSlave::tpl_err_msg(int i)
{
	string err_msg;
	switch (i)
	{
	case 0:
		err_msg = "Routine completed successfully";
		break;
	case 1:
		err_msg = "TPL file does not exist";
		break;
	case 2:
		err_msg = "Not all parameters listed in TPL file";
		break;
	case 3:
		err_msg = "Illegal header specified in TPL file";
		break;
	case 4:
		err_msg = "Error getting parameter name from template";
		break;
	case 5:
		err_msg = "Error getting parameter name from template";
		break;
	case 10:
		err_msg = "Error writing to model input file";
	}
	return err_msg;
}


string YAMRSlave::ins_err_msg(int i)
{
	string err_msg;
	switch (i)
	{
	case 0:
		err_msg = "Routine completed successfully";
		break;
	default:
		err_msg = "";
	}
	return err_msg;
}

int YAMRSlave::run_model(Parameters &pars, Observations &obs)
{
	int success = 1;
	//bool isDouble = true;
	//bool forceRadix = true;
	//TemplateFiles tpl_files(isDouble,forceRadix,tplfile_vec,inpfile_vec,par_name_vec);
	//InstructionFiles ins_files(insfile_vec,outfile_vec,obs_name_vec);
	std::vector<double> obs_vec;
	try 
	{
	//	message.str("");
		int ifail;
		int ntpl = tplfile_vec.size();
		int npar = pars.size();
		vector<string> par_name_vec;
		vector<double> par_values;
		for(auto &i : pars)
		{
			par_name_vec.push_back(i.first);
			par_values.push_back(i.second);
		}
		wrttpl_(&ntpl, StringvecFortranCharArray(tplfile_vec, 50).get_prt(),
			StringvecFortranCharArray(inpfile_vec, 50).get_prt(),
			&npar, StringvecFortranCharArray(par_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
			par_values.data(), &ifail);
		if(ifail != 0)
		{
			throw PestError("Error processing template file:" + tpl_err_msg(ifail));
		}
		//tpl_files.writtpl(par_values);

		// update parameter values
		pars.clear();
		for (int i=0; i<npar; ++i)
		{
			pars[par_name_vec[i]] = par_values[i];
		}

		// run model
		for (auto &i : comline_vec)
		{
			ifail = system(i.c_str());
			if(ifail != 0)
		{
			cerr << "Error executing command line: " << i << endl;
			throw PestError("Error executing command line: " + i);
		}
		}
		// process instructio files
		int nins = insfile_vec.size();
		int nobs = obs_name_vec.size();
		std::vector<double> obs_vec;
		obs_vec.resize(nobs, -9999.00);
		readins_(&nins, StringvecFortranCharArray(insfile_vec, 50).get_prt(),
			StringvecFortranCharArray(outfile_vec, 50).get_prt(),
			&nobs, StringvecFortranCharArray(obs_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
			obs_vec.data(), &ifail);
		if(ifail != 0)
		{
			throw PestError("Error processing template file");
		}
		//obs_vec = ins_files.readins();


		// check parameters and observations for inf and nan
		if (std::any_of(par_values.begin(), par_values.end(), OperSys::double_is_invalid))
		{
			throw PestError("Error running model: invalid parameter value returned");
		}
		if (std::any_of(obs_vec.begin(), obs_vec.end(), OperSys::double_is_invalid))
		{
			throw PestError("Error running model: invalid observation value returned");
		}
		// update observation values
		obs.clear();
		for (int i=0; i<nobs; ++i)
		{
			obs[obs_name_vec[i]] = obs_vec[i];
		}

	}
	catch(const std::exception& ex)
	{
		cerr << endl;
		cerr << "   " << ex.what() << endl;
		cerr << "   Aborting model run" << endl << endl;
		success = 0;
	}
	catch(...)
	{
		cerr << "   Error running model" << endl;
		cerr << "   Aborting model run" << endl;
		success = 0;
	}
	return success;
}


void YAMRSlave::start(const string &host, const string &port)
{
	NetPackage net_pack;
	Observations obs;
	Parameters pars;
	vector<char> serialized_data;
	int err;
	bool terminate = false;

	init_network(host, port);
	while (!terminate)
	{
		//get message from master
		err = recv_message(net_pack);
		if (err == -1) {}
		else if(net_pack.get_type() == NetPackage::PackType::REQ_RUNDIR)
		{
			// Send Master the local run directory.  This information is only used by the master
			// for reporting purposes
			net_pack.reset(NetPackage::PackType::RUNDIR, 0, 0,"");
			string cwd =  OperSys::getcwd();
			err = send_message(net_pack, cwd.c_str(), cwd.size());
		}
		else if(net_pack.get_type() == NetPackage::PackType::CMD)
		{
			vector<vector<string>> tmp_vec_vec;
			Serialization::unserialize(net_pack.get_data(), tmp_vec_vec);
			comline_vec = tmp_vec_vec[0];
			tplfile_vec = tmp_vec_vec[1];
			inpfile_vec = tmp_vec_vec[2];
			insfile_vec = tmp_vec_vec[3];
			outfile_vec = tmp_vec_vec[4];
			par_name_vec= tmp_vec_vec[5];
			obs_name_vec= tmp_vec_vec[6];
		}
		else if(net_pack.get_type() == NetPackage::PackType::REQ_LINPACK)
		{
			linpack_wrap();
			net_pack.reset(NetPackage::PackType::LINPACK, 0, 0,"");
			char data;
			err = send_message(net_pack, &data, 0);
		}
		else if(net_pack.get_type() == NetPackage::PackType::START_RUN)
		{
			Serialization::unserialize(net_pack.get_data(), pars, par_name_vec);
			// run model
			int group_id = net_pack.get_groud_id();
			int run_id = net_pack.get_run_id();
			cout << "received parameters (group id = " << group_id << ", run id = " << run_id << ")" << endl;
			cout << "starting model run..." << endl;
			if (run_model(pars, obs))
			{
				//send model results back
				cout << "run complete" << endl;
				cout << "sending results to master (group id = " << group_id << ", run id = " << run_id << ")..." <<endl;
				cout << "results sent" << endl << endl;
				serialized_data = Serialization::serialize(pars, par_name_vec, obs, obs_name_vec);
				net_pack.reset(NetPackage::PackType::RUN_FINISH, net_pack.get_groud_id(), net_pack.get_run_id(), "");
				err = send_message(net_pack, serialized_data.data(), serialized_data.size());
				// Send READY Message to master
				net_pack.reset(NetPackage::PackType::READY, 0, 0,"");
				char data;
				err = send_message(net_pack, &data, 0);
			}
			else
			{
				serialized_data.clear();
				serialized_data.push_back('\0');
				net_pack.reset(NetPackage::PackType::RUN_FAILED, net_pack.get_groud_id(), net_pack.get_run_id(), "");
				err = send_message(net_pack, serialized_data.data(), serialized_data.size());
				// Send READY Message to master
				net_pack.reset(NetPackage::PackType::READY, 0, 0,"");
				char data;
				err = send_message(net_pack, &data, 0);
				w_sleep(500);
			}
		}
		else if (net_pack.get_type() == NetPackage::PackType::TERMINATE)
		{
			terminate = true;
		}
		else 
		{
			cout << "received unsupported messaged type: " << int(net_pack.get_type()) << endl;
		}
		w_sleep(100);
	}
}

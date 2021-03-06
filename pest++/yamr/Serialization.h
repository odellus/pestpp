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

#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <vector>
#include <climits>
#include <string>

class Transformable;
class Parameters;
class Observations;

class Serialization
{
public:
	static std::vector<char> serialize(unsigned long data);
	static std::vector<char> serialize(const Transformable &tr_data);
	static std::vector<char> serialize(const std::vector<const Transformable*> tr_vec);
	static std::vector<char> serialize(const std::vector<Transformable*> &tr_vec);
	static std::vector<char> serialize(const Parameters &pars, const Observations &obs);
	static std::vector<char> serialize(const Parameters &pars, const std::vector<std::string> &par_names_vec, const Observations &obs, const std::vector<std::string> &obs_names_vec);
	static std::vector<char> serialize(const std::vector<std::string> &string_vec);
	static std::vector<char> serialize(const std::vector<std::vector<std::string> const*> &string_vec_vec);
	static unsigned long unserialize(const std::vector<char> &ser_data, unsigned long &data, unsigned long start_loc=0);
	static unsigned long unserialize(const std::vector<char> &ser_data, Transformable &tr_data, unsigned long start_loc=0);
	static unsigned long unserialize(const std::vector<char> &ser_data, std::vector<Transformable*> &tr_vec, unsigned long start_loc=0);
	static unsigned long unserialize(const std::vector<char> &ser_data, Parameters &pars, Observations &obs, unsigned long start_loc=0);
	static unsigned long unserialize(const std::vector<char> &ser_data, std::vector<std::string> &string_vec, unsigned long start_loc=0, unsigned long max_read_bytes=ULONG_MAX);
	static unsigned long unserialize(const std::vector<char> &ser_data, std::vector<std::vector<std::string>> &string_vec_vec);
	static unsigned long unserialize(const std::vector<char> &ser_data, Transformable &items, const std::vector<std::string> &names_vec, unsigned long start_loc = 0);
	static unsigned long unserialize(const std::vector<char> &ser_data, Parameters &pars, const std::vector<std::string> &par_names, Observations &obs, const std::vector<std::string> &obs_names);
private:
};


#endif /* SERIALIZE_H_ */





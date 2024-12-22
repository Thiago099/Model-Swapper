#include "Serialization.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>


void Serialization::saveDataBinary(const Data& data, const std::string& filename) {
    std::ofstream ofs(filename + ".bin", std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    boost::archive::binary_oarchive oa(ofs);
    oa << data;
}

void Serialization::loadDataBinary(Data& data, const std::string& filename) {
    std::ifstream ifs(filename + ".bin", std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    boost::archive::binary_iarchive ia(ifs);
    ia >> data;
}


Serialization::Data::Data(const std::map<RefID, inventory_stack>& inventory_variants, const std::map<RefID, v_variant>& worldobject_stacks) {
 
	for (const auto& [refid, inventory_] : inventory_variants) {
		for (const auto& [formid, variants] : inventory_) {
			for (const auto& variant : variants) {
				inventory[refid][formid].push_back(variant);
			}
		}
	}

	for (const auto& [refid, stack] : worldobject_stacks) {
		for (const auto& variant : stack) {
            worldobject[refid].push_back(variant);
		}
	}
}
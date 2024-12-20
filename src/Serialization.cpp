#include "Serialization.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>


void Serialization::saveDataBinary(const Data& data, const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    boost::archive::binary_oarchive oa(ofs);
    oa << data;
}

void Serialization::loadDataBinary(Data& data, const std::string& filename) {
	std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    boost::archive::binary_iarchive ia(ifs);
    ia >> data;
}


Serialization::Data::Data(const std::map<std::string, uint32_t>& modelpaths, const std::map<RefID, const variant*>& applied_variants, const std::map<RefID, inventory_stack>& inventory_variants) {

	for (const auto& [index, model] : modelpaths) {
		lookup[model] = index;
	}

    for (const auto& [refid, variant] : applied_variants) {
		if (!modelpaths.contains(variant->model)) continue;
		applied[refid] = modelpaths.at(variant->model);
	}
	for (const auto& [refid, inventory_] : inventory_variants) {
		for (const auto& [formid, variants] : inventory_) {
			for (const auto& variant : variants) {
				if (!modelpaths.contains(variant->model)) continue;
				inventory[refid][formid].push_back(modelpaths.at(variant->model));
			}
		}
	}
}
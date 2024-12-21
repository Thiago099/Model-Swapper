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


Serialization::Data::Data(const std::map<std::string, uint32_t>& modelpaths, const std::map<RefID, const variant*>& applied_variants, const std::map<RefID, inventory_stack>& inventory_variants, const std::map<RefID, v_variant>& worldobject_stacks) {

	for (const auto& [index, model] : modelpaths) {
		lookup[model] = index;
	}

    for (const auto& [refid, variant] : applied_variants) {
		std::string model_name = variant ? variant->model : "";
		if (!modelpaths.contains(model_name)) continue;
		applied[refid] = modelpaths.at(model_name);
	}
	for (const auto& [refid, inventory_] : inventory_variants) {
		for (const auto& [formid, variants] : inventory_) {
			for (const auto& variant : variants) {
				std::string model_name = variant ? variant->model : "";
				if (!modelpaths.contains(model_name)) continue;
				inventory[refid][formid].push_back(modelpaths.at(model_name));
			}
		}
	}

	for (const auto& [refid, stack] : worldobject_stacks) {
		for (const auto& variant : stack) {
			std::string model_name = variant ? variant->model : "";
			if (!modelpaths.contains(model_name)) continue;
			worldobject[refid].push_back(modelpaths.at(model_name));
		}
	}
}
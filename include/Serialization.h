#pragma once
#include "boost/serialization/access.hpp"
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include "Wrapper.h"

using inventory_stack = std::map<FormID, std::vector<const variant*>>;

namespace Serialization {


	inline const std::string serialization_path = "Data/SKSE/Plugins/ModelSwapper/Serialization/";

    inline const char* GetPath(const std::string& prefix, const char* filename) {
		const std::string path = prefix + filename;
		return path.c_str();
	}

	struct Data {
        std::map<uint32_t,std::string> lookup;
        std::map<uint32_t, uint32_t> applied;
        std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> inventory;

        // Make Boost.Serialization a friend so it can access private or protected members
        // if needed. For public members, this is not strictly necessary.
        friend class boost::serialization::access;

        // Implement a serialize function template
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version) {
            (void)version; // Silence unreferenced parameter warning
            ar & lookup;
            ar & applied;
            ar & inventory;
        }

		Data() = default;
		Data(const std::map<std::string, uint32_t>& modelpaths, const std::map<RefID, const variant*>& applied_variants, const std::map<RefID, inventory_stack>& inventory_variants);

    };
    void saveDataBinary(const Data &data, const std::string &filename);

    void loadDataBinary(Data &data, const std::string &filename);

}

BOOST_CLASS_VERSION(Serialization::Data, 1);
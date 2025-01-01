#pragma once
#include "boost/serialization/access.hpp"
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include "Application/Model.h"

namespace Serialization {


	inline const std::string serialization_path = "Data/SKSE/Plugins/ModelSwapper/Serialization/";

	struct Data {
        std::map<uint32_t, int32_t> applied;
        std::map<uint32_t, std::map<uint32_t, std::vector<int32_t>>> inventory;
		std::map<uint32_t, std::vector<int32_t>> worldobject;

        // Make Boost.Serialization a friend so it can access private or protected members
        // if needed. For public members, this is not strictly necessary.
        friend class boost::serialization::access;

        // Implement a serialize function template
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version) {
            (void)version; // Silence unreferenced parameter warning
            ar & inventory;
			ar& worldobject;
        }

		Data() = default;
		Data(
            const std::map<RefID, inventory_stack>& inventory_variants,
			const std::map<RefID, v_variant>& worldobject_stacks);

    };

    void LoadSerializedData(const char* filename);

    void SerializeData(const char* filename);

}

BOOST_CLASS_VERSION(Serialization::Data, 1);
#include "Adaptors/Serialization.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "Application/InventoryManager.h"
#include "Application/WorldStackManager.h"
namespace Serialization {
    void saveDataBinary(const Data& data, const std::string& filename) {
        std::ofstream ofs(filename + ".bin", std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }

        boost::archive::binary_oarchive oa(ofs);
        oa << data;
    }

    void loadDataBinary(Data& data, const std::string& filename) {
        std::ifstream ifs(filename + ".bin", std::ios::binary);
        if (!ifs) {
            throw std::runtime_error("Failed to open file for reading: " + filename);
        }

        boost::archive::binary_iarchive ia(ifs);
        ia >> data;
    }
}


void Serialization::LoadSerializedData(const char* filename) {
    logger::info("Loading data from {}", filename);


    Serialization::Data saved_data;
    Serialization::loadDataBinary(saved_data, filename);

    {
        auto inventory = InventoryManager::GetSingleton();
        inventory->ClearData();
        std::unique_lock lock_var(inventory->GetMutex());

        for (const auto& [owner_refid, item_map] : saved_data.inventory) {
            for (const auto& [item_refid, model_indices] : item_map) {
                for (const auto model_index : model_indices) {
                    inventory->Add(owner_refid, item_refid, model_index);
                }
            }
        }
    }

    {
        auto worldStack = WorldStackManager::GetSingleton();
        worldStack->Clean();
        std::unique_lock lock_inv(worldStack->GetMutex());
        for (const auto& [owner_refid, model_indices] : saved_data.worldobject) {
            for (const auto model_index : model_indices) {
                worldStack->Add(owner_refid, model_index);
            }
        }
    }

}

void Serialization::SerializeData(const char* filename) {
    const auto file_path = Serialization::serialization_path + filename;

    auto worldStack = WorldStackManager::GetSingleton();
    auto inventory = InventoryManager::GetSingleton();

    std::shared_lock lock_inv(inventory->GetMutex());
    std::shared_lock lock_var(worldStack->GetMutex());

    const Serialization::Data data(inventory->GetAll(), worldStack->GetAll());

    Serialization::saveDataBinary(data, file_path);
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
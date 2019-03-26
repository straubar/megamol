#include "stdafx.h"
#include "mesh_data_call.h"

#include <memory>
#include <string>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        void mesh_data_call::set_data(const std::string& name, std::shared_ptr<data_set> data)
        {
            if (this->data_sets.find(name) == this->data_sets.end() || data != nullptr)
            {
                this->data_sets[name] = data;
            }
        }

        std::shared_ptr<mesh_data_call::data_set> mesh_data_call::get_data(const std::string& name) const
        {
            if (this->data_sets.find(name) != this->data_sets.end())
            {
                return this->data_sets.at(name);
            }

            return nullptr;
        }

        std::vector<std::string> mesh_data_call::get_data_sets() const
        {
            std::vector<std::string> data_sets;

            for (const auto& entry : this->data_sets)
            {
                data_sets.push_back(entry.first);
            }

            return data_sets;
        }
    }
}

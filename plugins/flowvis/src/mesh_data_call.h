/*
 * mesh_data_call.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */
#pragma once

#include "mmcore/AbstractGetDataCall.h"
#include "mmcore/factories/CallAutoDescription.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        /**
        * Call for transporting scalar data, attached with a transfer function, in an ready-to-use fashion (for OpenGL).
        *
        * @author Alexander Straub
        */
        class mesh_data_call : public core::AbstractGetDataCall
        {
        public:
            typedef core::factories::CallAutoDescription<mesh_data_call> mesh_data_description;

            /** Struct to store data and relevant information for visualization */
            struct data_set
            {
                std::string transfer_function;
                bool transfer_function_dirty;

                float min_value, max_value;

                std::shared_ptr<std::vector<float>> data;
            };

            /**
            * Human-readable class name
            */
            static const char* ClassName() { return "mesh_data_call"; }

            /**
            * Human-readable class description
            */
            static const char *Description() { return "Call transporting data stored in a mesh"; }

            /**
            * Number of available functions
            */
            static unsigned int FunctionCount() { return 2; }

            /**
            * Names of available functions
            */
            static const char * FunctionName(unsigned int idx)
            {
                switch (idx)
                {
                case 0: return "get_data";
                case 1: return "get_extent";
                }

                return nullptr;
            }

            /**
            * Set the data for a given name
            *
            * @param name Name of the data set
            * @param data Data set
            */
            void set_data(const std::string& name, std::shared_ptr<data_set> data = nullptr);

            /**
            * Get the data, as indicated by the name
            *
            * @param name Name of the data set
            *
            * @return Data set, or nullptr if it does not exist
            */
            std::shared_ptr<data_set> get_data(const std::string& name) const;

            /**
            * Get data set names
            *
            * @return Names of all available data sets
            */
            std::vector<std::string> get_data_sets() const;

        protected:
            /** Store data sets with their name */
            std::map<std::string, std::shared_ptr<data_set>> data_sets;
        };
    }
}
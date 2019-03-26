/*
 * implicit_topology.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */
#pragma once

#include "implicit_topology_computation.h"
#include "triangulation.h"

#include "mmcore/Call.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"
#include "mmcore/param/ParamSlot.h"

#include "glad/glad.h"

#include <memory>
#include <type_traits>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        /**
        * Module for computing and visualizing the implicit topology of a vector field.
        *
        * @author Alexander Straub
        * @author Grzegorz K. Karch
        */
        class implicit_topology : public core::Module
        {
            static_assert(std::is_same<GLfloat, float>::value, "'GLfloat' and 'float' must be the same type!");
            static_assert(std::is_same<GLuint, unsigned int>::value, "'GLuint' and 'unsigned int' must be the same type!");

        public:
            /**
             * Answer the name of this module.
             *
             * @return The name of this module.
             */
            static inline const char* ClassName() { return "implicit_topology"; }

            /**
             * Answer a human readable description of this module.
             *
             * @return A human readable description of this module.
             */
            static inline const char* Description() { return "Compute and visualize implicit topology of a 2D vector field"; }

            /**
             * Answers whether this module is available on the current system.
             *
             * @return 'true' if the module is available, 'false' otherwise.
             */
            static inline bool IsAvailable() { return true; }

            /**
             * Initialises a new instance.
             */
            implicit_topology();

            /**
             * Finalises an instance.
             */
            virtual ~implicit_topology();

        protected:
            /**
             * Implementation of 'Create'.
             *
             * @return 'true' on success, 'false' otherwise.
             */
            virtual bool create() override;

            /**
             * Implementation of 'Release'.
             */
            virtual void release() override;

        private:
            /** Callbacks for the triangle mesh */
            bool get_triangle_data_callback(core::Call& call);
            bool get_triangle_extent_callback(core::Call& call);

            /** Callbacks for the mesh data */
            bool get_data_data_callback(core::Call& call);
            bool get_data_extent_callback(core::Call& call);

            /** Output slot for the triangle mesh */
            core::CalleeSlot triangle_mesh_slot;

            /** Output slot for data attached to the triangles or their nodes */
            core::CalleeSlot mesh_data_slot;

            /** Path to input vector field */
            core::param::ParamSlot vector_field_path;

            /** Path to input convergence structures */
            core::param::ParamSlot convergence_structures_path;
            
            /** Transfer function for labels */
            core::param::ParamSlot label_transfer_function;
            
            /** Transfer function for distances */
            core::param::ParamSlot distance_transfer_function;

            /** Indicator for changed output */
            bool output_changed;

            /** Output labels */
            std::shared_ptr<std::vector<GLfloat>> labels;

            /** Output distances */
            std::shared_ptr<std::vector<GLfloat>> distances;

            /** Computation class */
            std::unique_ptr<implicit_topology_computation> computation;

            /** Store last promised result */
            std::shared_future<implicit_topology_computation::result> last_result;
        };
    }
}
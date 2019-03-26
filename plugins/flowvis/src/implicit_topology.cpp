#include "stdafx.h"
#include "implicit_topology.h"

#include "implicit_topology_computation.h"
#include "mesh_data_call.h"
#include "triangle_mesh_call.h"
#include "triangulation.h"

#include "mmcore/Call.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/param/LinearTransferFunctionParam.h"

#include "vislib/math/Rectangle.h"
#include "vislib/sys/Log.h"

#include "glad/glad.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        implicit_topology::implicit_topology() :
            triangle_mesh_slot("set_triangle_mesh", "Triangle mesh output"),
            mesh_data_slot("set_mesh_data", "Mesh data output"),
            vector_field_path("vector_field_path", "Path to the input vector field"),
            convergence_structures_path("convergence_structures_path", "Path to the input convergence structures"),
            label_transfer_function("label_transfer_function", "Transfer function for labels"),
            distance_transfer_function("distance_transfer_function", "Transfer function for distances"),
            output_changed(false), computation(nullptr)
        {
            // Connect output
            this->triangle_mesh_slot.SetCallback(triangle_mesh_call::ClassName(), triangle_mesh_call::FunctionName(0), &implicit_topology::get_triangle_data_callback);
            this->triangle_mesh_slot.SetCallback(triangle_mesh_call::ClassName(), triangle_mesh_call::FunctionName(1), &implicit_topology::get_triangle_extent_callback);
            this->MakeSlotAvailable(&this->triangle_mesh_slot);

            this->mesh_data_slot.SetCallback(mesh_data_call::ClassName(), mesh_data_call::FunctionName(0), &implicit_topology::get_data_data_callback);
            this->mesh_data_slot.SetCallback(mesh_data_call::ClassName(), mesh_data_call::FunctionName(1), &implicit_topology::get_data_extent_callback);
            this->MakeSlotAvailable(&this->mesh_data_slot);

            // Create parameters
            this->vector_field_path << new core::param::FilePathParam("");
            this->MakeSlotAvailable(&this->vector_field_path);

            this->convergence_structures_path << new core::param::FilePathParam("");
            this->MakeSlotAvailable(&this->convergence_structures_path);

            this->label_transfer_function << new core::param::LinearTransferFunctionParam();
            this->MakeSlotAvailable(&this->label_transfer_function);

            this->distance_transfer_function << new core::param::LinearTransferFunctionParam();
            this->MakeSlotAvailable(&this->distance_transfer_function);
        }

        implicit_topology::~implicit_topology()
        {
            this->Release();
        }

        bool implicit_topology::create()
        {
            return true;
        }

        void implicit_topology::release()
        {
        }

        bool implicit_topology::get_triangle_data_callback(core::Call& call)
        {
            auto* triangle_call = dynamic_cast<triangle_mesh_call*>(&call);
            if (triangle_call == nullptr) return false;

            // Start computation
            if (this->vector_field_path.IsDirty() || convergence_structures_path.IsDirty())
            {
                // Reset
                this->vector_field_path.ResetDirty();
                this->convergence_structures_path.ResetDirty();

                triangle_call->SetDataHash(0);

                // Terminate earlier computation
                if (this->computation != nullptr)
                {
                    this->computation->terminate();
                }

                // Try to load input vector field
                std::ifstream vectors_ifs(this->vector_field_path.Param<core::param::FilePathParam>()->Value(), std::ios_base::in | std::ios_base::binary);
                std::ifstream structures_ifs(this->convergence_structures_path.Param<core::param::FilePathParam>()->Value(), std::ios_base::in | std::ios_base::binary);

                if (vectors_ifs.good() && structures_ifs.good())
                {
                    // Get dimension from file
                    unsigned int dimension, components;

                    vectors_ifs.read(reinterpret_cast<char*>(&dimension), sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&components), sizeof(unsigned int));

                    if (dimension != 2)
                    {
                        vislib::sys::Log::DefaultLog.WriteError("Vector field file must have exactly two dimensions '%s'",
                            this->vector_field_path.Param<core::param::FilePathParam>()->Value());

                        return false;
                    }

                    if (components != 2)
                    {
                        vislib::sys::Log::DefaultLog.WriteError("Vectors must have exactly two components '%s'",
                            this->vector_field_path.Param<core::param::FilePathParam>()->Value());

                        return false;
                    }

                    // Read extents from file
                    float x_min, x_max, y_min, y_max;
                    unsigned int x_num, y_num, num;

                    vectors_ifs.read(reinterpret_cast<char*>(&x_num), sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&x_min), sizeof(float));
                    vectors_ifs.read(reinterpret_cast<char*>(&x_max), sizeof(float));
                    vectors_ifs.read(reinterpret_cast<char*>(&y_num), sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&y_min), sizeof(float));
                    vectors_ifs.read(reinterpret_cast<char*>(&y_max), sizeof(float));

                    num = x_num * y_num;

                    // Read file content
                    const float x_step = (x_max - x_min) / (x_num - 1);
                    const float y_step = (y_max - y_min) / (y_num - 1);

                    std::vector<GLfloat> positions(num * 2);
                    std::vector<GLfloat> vectors(num * 3);

                    for (unsigned int y = 0; y < y_num; ++y)
                    {
                        for (unsigned int x = 0; x < x_num; ++x)
                        {
                            const unsigned int xy = y * x_num + x;

                            // Calculate positions
                            const float x_pos = x_min + x * x_step;
                            const float y_pos = y_min + y * y_step;

                            positions[xy * 2 + 0] = x_pos;
                            positions[xy * 2 + 1] = y_pos;

                            // Read vectors
                            vectors_ifs.read(reinterpret_cast<char*>(&vectors[xy * 3 + 0]), sizeof(float));
                            vectors_ifs.read(reinterpret_cast<char*>(&vectors[xy * 3 + 1]), sizeof(float));
                            vectors_ifs.read(reinterpret_cast<char*>(&vectors[xy * 3 + 2]), sizeof(float));
                        }
                    }

                    vectors_ifs.close();

                    // Load convergence structures
                    unsigned int num_convergence_structures;

                    structures_ifs.read(reinterpret_cast<char*>(&num_convergence_structures), sizeof(unsigned int));

                    std::vector<GLfloat> points, lines;
                    std::vector<int> point_ids, line_ids;

                    points.reserve(2 * num_convergence_structures);
                    lines.reserve(4 * num_convergence_structures);

                    point_ids.reserve(num_convergence_structures);
                    line_ids.reserve(num_convergence_structures);

                    for (unsigned int i = 0; i < num_convergence_structures; ++i)
                    {
                        unsigned int type;
                        structures_ifs.read(reinterpret_cast<char*>(&type), sizeof(unsigned int));

                        float value;

                        switch (type)
                        {
                        case 0: // Point
                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            points.push_back(value);
                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            points.push_back(value);

                            point_ids.push_back(i);

                            break;
                        case 1: // Line
                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            lines.push_back(value);
                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            lines.push_back(value);

                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            lines.push_back(value);
                            structures_ifs.read(reinterpret_cast<char*>(&value), sizeof(float));
                            lines.push_back(value);

                            line_ids.push_back(i);

                            break;
                        default:
                            vislib::sys::Log::DefaultLog.WriteError("Unknown convergence structure type in file '%s'!", 
                                this->convergence_structures_path.Param<core::param::FilePathParam>()->Value());

                            return false;
                        }
                    }

                    // Start asynchronous computation
                    this->computation = std::make_unique<implicit_topology_computation>(std::move(positions), std::move(vectors),
                        std::move(points), std::move(point_ids), std::move(lines), std::move(line_ids));

                    this->computation->start();

                    this->last_result = this->computation->get_results();

                    triangle_call->SetDataHash(1);
                    this->output_changed = true;
                }
                else if (!vectors_ifs.good())
                {
                    vislib::sys::Log::DefaultLog.WriteWarn("Unable to open input vector field file '%s'!",
                        this->vector_field_path.Param<core::param::FilePathParam>()->Value());

                    return false;
                }
                else if (!structures_ifs.good())
                {
                    vislib::sys::Log::DefaultLog.WriteWarn("Unable to open input convergence structures file '%s'!",
                        this->convergence_structures_path.Param<core::param::FilePathParam>()->Value());

                    return false;
                }
            }

            // Update render output while computation is still running
            if (triangle_call->DataHash() != 0 && this->computation != nullptr)
            {
                // Get new results
                if (this->last_result.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready)
                {
                    return true;
                }

                // Store triangles
                auto result = this->last_result.get();

                triangle_call->set_vertices(result.vertices);
                triangle_call->set_indices(result.indices);

                this->labels = result.labels;
                this->distances = result.distances;

                // Check state of computation
                if (result.finished)
                {
                    triangle_call->SetDataHash(0);
                }
                else
                {
                    triangle_call->SetDataHash(triangle_call->DataHash() + 1);
                }

                // Save new last result
                this->last_result = this->computation->get_results();

                this->output_changed = true;
            }
            
            return true;
        }

        bool implicit_topology::get_triangle_extent_callback(core::Call& call)
        {
            auto* triangle_call = dynamic_cast<triangle_mesh_call*>(&call);
            if (triangle_call == nullptr) return false;

            if (this->vector_field_path.IsDirty())
            {
                // Try to load input vector field
                std::ifstream vectors_ifs(this->vector_field_path.Param<core::param::FilePathParam>()->Value(), std::ios_base::in | std::ios_base::binary);

                if (vectors_ifs.good())
                {
                    // Read dimension from file
                    unsigned int dimension, components;

                    vectors_ifs.read(reinterpret_cast<char*>(&dimension), sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&components), sizeof(unsigned int));

                    if (dimension != 2)
                    {
                        vislib::sys::Log::DefaultLog.WriteError("Vector field file must have exactly two dimensions '%s'",
                            this->vector_field_path.Param<core::param::FilePathParam>()->Value());

                        return false;
                    }

                    if (components != 2)
                    {
                        vislib::sys::Log::DefaultLog.WriteError("Vectors must have exactly two components '%s'",
                            this->vector_field_path.Param<core::param::FilePathParam>()->Value());

                        return false;
                    }

                    // Read extents from file
                    float x_min, x_max, y_min, y_max;

                    vectors_ifs.ignore(sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&x_min), sizeof(float));
                    vectors_ifs.read(reinterpret_cast<char*>(&x_max), sizeof(float));
                    vectors_ifs.ignore(sizeof(unsigned int));
                    vectors_ifs.read(reinterpret_cast<char*>(&y_min), sizeof(float));
                    vectors_ifs.read(reinterpret_cast<char*>(&y_max), sizeof(float));

                    triangle_call->set_bounding_rectangle(vislib::math::Rectangle<float>(x_min, y_min, x_max, y_max));
                }
                else
                {
                    triangle_call->SetDataHash(0);

                    this->vector_field_path.ResetDirty();

                    return false;
                }
            }

            return true;
        }

        bool implicit_topology::get_data_data_callback(core::Call& call)
        {
            auto* data_call = dynamic_cast<mesh_data_call*>(&call);
            if (data_call == nullptr) return false;

            if (this->output_changed)
            {
                // Prepare labels
                {
                    auto label_data = std::make_shared<mesh_data_call::data_set>();
                    label_data->transfer_function = this->label_transfer_function.Param<core::param::LinearTransferFunctionParam>()->Value();

                    const auto min_max_value = std::minmax_element(this->labels->begin(), this->labels->end());
                    label_data->min_value = *min_max_value.first;
                    label_data->max_value = *min_max_value.second;

                    label_data->data = this->labels;

                    data_call->set_data("labels", label_data);
                }
                
                // Prepare distances
                {
                    auto distance_data = std::make_shared<mesh_data_call::data_set>();
                    distance_data->transfer_function = this->distance_transfer_function.Param<core::param::LinearTransferFunctionParam>()->Value();
                    
                    const auto min_max_value = std::minmax_element(this->distances->begin(), this->distances->end());
                    distance_data->min_value = *min_max_value.first;
                    distance_data->max_value = *min_max_value.second;

                    distance_data->data = this->distances;

                    data_call->set_data("distances", distance_data);
                }

                // Set new data hash
                data_call->SetDataHash(data_call->DataHash() + 1);

                this->output_changed = false;
            }

            if (this->label_transfer_function.IsDirty())
            {
                auto label_data = data_call->get_data("labels");
                
                if (label_data != nullptr)
                {
                    label_data->transfer_function = this->label_transfer_function.Param<core::param::LinearTransferFunctionParam>()->Value();
                    label_data->transfer_function_dirty = true;
                }

                this->label_transfer_function.ResetDirty();
            }
            if (this->distance_transfer_function.IsDirty())
            {
                auto distance_data = data_call->get_data("distances");

                if (distance_data != nullptr)
                {
                    distance_data->transfer_function = this->distance_transfer_function.Param<core::param::LinearTransferFunctionParam>()->Value();
                    distance_data->transfer_function_dirty = true;
                }

                this->distance_transfer_function.ResetDirty();
            }

            return true;
        }

        bool implicit_topology::get_data_extent_callback(core::Call& call)
        {
            auto* data_call = dynamic_cast<mesh_data_call*>(&call);
            if (data_call == nullptr) return false;

            data_call->set_data("labels");
            data_call->set_data("distances");

            return true;
        }

    }
}

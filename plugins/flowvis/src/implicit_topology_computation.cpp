#include "stdafx.h"
#include "implicit_topology_computation.h"

#include "../cuda/streamlines.h"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <utility>

namespace megamol
{
    namespace flowvis
    {
        implicit_topology_computation::implicit_topology_computation(std::ostream& log_stream, std::ostream& performance_stream, 
            std::array<int, 2> resolution, std::array<float, 4> domain, std::vector<float> positions, std::vector<float> vectors,
            std::vector<float> points, std::vector<int> point_ids, std::vector<float> lines, std::vector<int> line_ids,
            const float integration_timestep, const float max_integration_error)
            : log_output(log_stream), performance_output(performance_stream), resolution(std::move(resolution)),
            domain(std::move(domain)), positions(std::move(positions)), vectors(std::move(vectors)),
            points(std::move(points)), point_ids(std::move(point_ids)), lines(std::move(lines)),
            line_ids(std::move(line_ids)), integration_timestep(integration_timestep), max_integration_error(max_integration_error),
            num_integration_steps_performed(0), terminate_computation(false)
        {
            this->log_output << "Initializing computation..." << std::endl;
            this->log_output << "Resolution:                            " << this->resolution[0] << " x " << this->resolution[1] << std::endl;
            this->log_output << "Domain:                                " << "[" << this->domain[0] << ", " << this->domain[1] << "] x [" << this->domain[2] << ", " << this->domain[3] << "]" << std::endl;
            this->log_output << "Number of convergence points:          " << this->point_ids.size() << std::endl;
            this->log_output << "Number of convergence lines:           " << this->line_ids.size() << std::endl;
            this->log_output << "Integration time step:                 " << this->integration_timestep << std::endl;
            this->log_output << "Maximum integration error:             " << this->max_integration_error << std::endl;
            this->log_output << std::endl;

            // Store positions
            this->positions_forward = this->positions;
            this->positions_backward = this->positions;

            // Compute initial fields
            unsigned int num = this->resolution[0] * this->resolution[1];

            this->labels_forward.resize(num);
            this->distances_forward.resize(num);
            this->terminations_forward.resize(num);

            this->labels_backward.resize(num);
            this->distances_backward.resize(num);
            this->terminations_backward.resize(num);

            auto calc_dot = [](const float x_1, const float y_1, const float x_2, const float y_2) { return x_1 * x_2 + y_1 * y_2; };
            auto calc_norm = [calc_dot](const float x, const float y) { return calc_dot(x, y, x, y); };
            auto calc_length = [calc_norm](const float x_1, const float y_1, const float x_2, const float y_2) { return std::sqrt(calc_norm(x_1 - x_2, y_1 - y_2)); };

            for (unsigned int n = 0; n < num; ++n)
            {
                const float x_pos = this->positions[n * 2 + 0];
                const float y_pos = this->positions[n * 2 + 1];

                const float x_vec = this->vectors[n * 2 + 0];
                const float y_vec = this->vectors[n * 2 + 1];

                this->distances_forward[n] = this->distances_backward[n] = std::numeric_limits<float>::max();

                // Compute minimum distance to convergence structures represented by points
                for (unsigned int i = 0; i < this->point_ids.size(); ++i)
                {
                    const float point_x_pos = this->points[i * 2 + 0];
                    const float point_y_pos = this->points[i * 2 + 1];

                    const float distance = calc_length(x_pos, y_pos, point_x_pos, point_y_pos);

                    if (this->distances_forward[n] > distance)
                    {
                        this->labels_forward[n] = this->labels_backward[n] = static_cast<GLfloat>(this->point_ids[i]);
                        this->distances_forward[n] = this->distances_backward[n] = distance;
                    }
                }

                // Compute minimum distance to convergence structures represented by lines
                for (unsigned int i = 0; i < this->line_ids.size(); ++i)
                {
                    float distance = 0.0f;

                    const float line_1_x_pos = this->lines[i * 4 + 0];
                    const float line_1_y_pos = this->lines[i * 4 + 1];
                    const float line_2_x_pos = this->lines[i * 4 + 2];
                    const float line_2_y_pos = this->lines[i * 4 + 3];

                    const float length = calc_length(line_1_x_pos, line_1_y_pos, line_2_x_pos, line_2_y_pos);

                    const float line_vector_x = length == 0.0f ? 0.0f : (line_2_x_pos - line_1_x_pos) / length;
                    const float line_vector_y = length == 0.0f ? 0.0f : (line_2_y_pos - line_1_y_pos) / length;

                    if (line_vector_x == 0.0f && line_vector_y == 0.0f)
                    {
                        distance = calc_length(x_pos, y_pos, line_1_x_pos, line_1_y_pos);
                    }
                    else
                    {
                        const float point_distance = calc_dot(line_vector_x, line_vector_y, x_pos - line_1_x_pos, y_pos - line_1_y_pos);

                        if (point_distance < 0.0f)
                        {
                            distance = calc_length(x_pos, y_pos, line_1_x_pos, line_1_y_pos);
                        }
                        else if (point_distance > length)
                        {
                            distance = calc_length(x_pos, y_pos, line_2_x_pos, line_2_y_pos);
                        }
                        else
                        {
                            const float projection_x_pos = line_1_x_pos + line_vector_x * point_distance;
                            const float projection_y_pos = line_1_y_pos + line_vector_y * point_distance;

                            distance = calc_length(x_pos, y_pos, projection_x_pos, projection_y_pos);
                        }
                    }

                    if (this->distances_forward[n] > distance)
                    {
                        this->labels_forward[n] = this->labels_backward[n] = static_cast<GLfloat>(this->line_ids[i]);
                        this->distances_forward[n] = this->distances_backward[n] = distance;
                    }
                }

                // Set special values if it is part of the boundary
                if (x_vec == 0.0f && y_vec == 0.0f)
                {
                    this->labels_forward[n] = this->labels_backward[n] = -1.0f;
                    this->distances_forward[n] = this->distances_backward[n] = 0.0f;
                    this->terminations_forward[n] = this->terminations_backward[n] = -1.0f;
                }
            }

            // Initialize triangulation
            this->delaunay.insert_points(this->positions);
        }

        implicit_topology_computation::implicit_topology_computation(std::ostream& log_stream, std::ostream& performance_stream,
            std::array<int, 2> resolution, std::array<float, 4> domain, std::vector<float> positions, std::vector<float> vectors,
            std::vector<float> points, std::vector<int> point_ids, std::vector<float> lines, std::vector<int> line_ids, result previous_result)
            : log_output(log_stream), performance_output(performance_stream), resolution(std::move(resolution)), domain(std::move(domain)),
            positions(std::move(positions)), vectors(std::move(vectors)), points(std::move(points)), point_ids(std::move(point_ids)),
            lines(std::move(lines)), line_ids(std::move(line_ids)), integration_timestep(previous_result.computation_state.integration_timestep),
            max_integration_error(previous_result.computation_state.max_integration_error),
            num_integration_steps_performed(previous_result.computation_state.num_integration_steps), terminate_computation(false),
            positions_forward(*previous_result.positions_forward), positions_backward(*previous_result.positions_backward),
            labels_forward(*previous_result.labels_forward), distances_forward(*previous_result.distances_forward),
            terminations_forward(*previous_result.terminations_forward), labels_backward(*previous_result.labels_backward),
            distances_backward(*previous_result.distances_backward), terminations_backward(*previous_result.terminations_backward),
            delaunay(*previous_result.vertices)
        {
            this->log_output << "Initializing computation from previous results..." << std::endl;
            this->log_output << "Resolution:                            " << this->resolution[0] << " x " << this->resolution[1] << std::endl;
            this->log_output << "Domain:                                " << "[" << this->domain[0] << ", " << this->domain[1] << "] x [" << this->domain[2] << ", " << this->domain[3] << "]" << std::endl;
            this->log_output << "Number of convergence points:          " << this->point_ids.size() << std::endl;
            this->log_output << "Number of convergence lines:           " << this->line_ids.size() << std::endl;
            this->log_output << "Integration time step:                 " << this->integration_timestep << std::endl;
            this->log_output << "Maximum integration error:             " << this->max_integration_error << std::endl;
            this->log_output << "Previously performed steps:            " << this->num_integration_steps_performed << std::endl;
            this->log_output << std::endl;
        }

        implicit_topology_computation::~implicit_topology_computation()
        {
            terminate();
        }

        void implicit_topology_computation::start(const unsigned int num_integration_steps,
            const float refinement_threshold, const bool refine_at_labels, const float distance_difference_threshold,
            const unsigned int num_particles_per_batch, const unsigned int num_integration_steps_per_batch)
        {
            // Prepare results
            {
                std::promise<result> promise;
                this->current_result = promise.get_future().share();

                // Set initial result
                set_result(promise, num_integration_steps <= this->num_integration_steps_performed);

                // Check if there is actually something to do
                if (num_integration_steps <= this->num_integration_steps_performed)
                {
                    return;
                }
            }

            // Start computation
            std::promise<result> promise;
            this->current_result = promise.get_future().share();

            if (this->computation.joinable())
            {
                this->computation.join();
            }

            this->computation = std::thread(&implicit_topology_computation::run, this, std::move(promise),
                num_integration_steps, refinement_threshold, refine_at_labels, distance_difference_threshold,
                num_particles_per_batch, num_integration_steps_per_batch);
        }

        void implicit_topology_computation::terminate()
        {
            this->terminate_computation = true;

            if (this->computation.joinable())
            {
                this->computation.join();
            }

            this->terminate_computation = false;
        }

        std::shared_future<implicit_topology_computation::result> implicit_topology_computation::get_results() const
        {
            return this->current_result;
        }

        void implicit_topology_computation::run(std::promise<result>&& promise, const unsigned int num_integration_steps,
            const float refinement_threshold, const bool refine_at_labels, const float distance_difference_threshold,
            const unsigned int num_particles_per_batch, const unsigned int num_integration_steps_per_batch)
        {
            this->log_output << "Starting computation..." << std::endl;
            this->log_output << "Target number of integration steps:    " << num_integration_steps << std::endl;
            this->log_output << "Already performed integration steps:   " << this->num_integration_steps_performed << std::endl;
            this->log_output << "Number of integration steps:           " << num_integration_steps - this->num_integration_steps_performed << std::endl;
            this->log_output << "Number of particles per batch:         " << num_particles_per_batch << std::endl;
            this->log_output << "Number of integration steps per batch: " << num_integration_steps_per_batch << std::endl;
            this->log_output << std::endl;
            this->log_output << "Integrating stream lines..." << std::endl;

            streamlines_cuda streamlines(this->resolution, this->domain, this->vectors, this->points, this->point_ids,
                this->lines, this->line_ids, this->integration_timestep, this->max_integration_error);

            bool finished = false;
            bool finished_integration = false;
            bool finished_refined_integration = true;

            std::vector<float> new_positions_forward;
            std::vector<float> new_positions_backward;

            std::vector<float> new_labels_forward;
            std::vector<float> new_labels_backward;

            std::vector<float> new_distances_forward;
            std::vector<float> new_distances_backward;

            std::vector<float> new_terminations_forward;
            std::vector<float> new_terminations_backward;

            unsigned int num_refined_integration_steps;

            while (!finished && !this->terminate_computation)
            {
                // Integrate stream lines
                if (!finished_integration)
                {
                    const unsigned int num_steps = std::min(num_integration_steps - this->num_integration_steps_performed, num_integration_steps_per_batch);

                    this->log_output << "Number of integration steps:           " << num_steps << "   " << this->num_integration_steps_performed << " / " << num_integration_steps << std::endl;

                    streamlines.update_labels(this->positions_forward, this->labels_forward, this->distances_forward, this->terminations_forward,
                        num_steps, 1.0f, num_particles_per_batch);

                    streamlines.update_labels(this->positions_backward, this->labels_backward, this->distances_backward, this->terminations_backward,
                        num_steps, -1.0f, num_particles_per_batch);

                    this->num_integration_steps_performed += num_steps;

                    // Check if all integration steps have been performed
                    if (this->num_integration_steps_performed >= num_integration_steps)
                    {
                        this->log_output << "Finished integration!" << std::endl << std::endl;

                        finished_integration = true;
                    }
                }

                // Refine grid
                if (finished_integration)
                {
                    if (finished_refined_integration)
                    {
                        // Refine grid and get new seed points
                        new_positions_forward = new_positions_backward = refine_grid(refinement_threshold, refine_at_labels, distance_difference_threshold);

                        // Check if new points have been added; if not, the computation is finished
                        if (new_positions_forward.empty())
                        {
                            finished = true;
                        }
                        else
                        {
                            // Create output fields
                            new_labels_forward.resize(new_positions_forward.size() / 2);
                            new_labels_backward.resize(new_positions_backward.size() / 2);

                            new_distances_forward.resize(new_positions_forward.size() / 2);
                            new_distances_backward.resize(new_positions_backward.size() / 2);

                            new_terminations_forward.resize(new_positions_forward.size() / 2);
                            new_terminations_backward.resize(new_positions_backward.size() / 2);

                            // Initialize output fields
                            std::fill(new_labels_forward.begin(), new_labels_forward.end(), -1.0f);
                            std::fill(new_labels_backward.begin(), new_labels_backward.end(), -1.0f);

                            std::fill(new_distances_forward.begin(), new_distances_forward.end(), std::numeric_limits<float>::max());
                            std::fill(new_distances_backward.begin(), new_distances_backward.end(), std::numeric_limits<float>::max());

                            std::fill(new_terminations_forward.begin(), new_terminations_forward.end(), 0.0f);
                            std::fill(new_terminations_backward.begin(), new_terminations_backward.end(), 0.0f);

                            finished_refined_integration = false;

                            num_refined_integration_steps = 0;

                            this->log_output << "Integrating stream lines for new particles..." << std::endl;
                        }
                    }
                    else
                    {
                        // Integrate stream lines for newly added seed
                        const unsigned int num_steps = std::min(num_integration_steps - num_refined_integration_steps, num_integration_steps_per_batch);

                        this->log_output << "Number of integration steps:           " << num_steps << "   " << num_refined_integration_steps << " / " << num_integration_steps << std::endl;

                        streamlines.update_labels(new_positions_forward, new_labels_forward, new_distances_forward, new_terminations_forward,
                            num_steps, 1.0f, num_particles_per_batch);

                        streamlines.update_labels(new_positions_backward, new_labels_backward, new_distances_backward, new_terminations_backward,
                            num_steps, -1.0f, num_particles_per_batch);

                        num_refined_integration_steps += num_steps;

                        // Check if all integration steps have been performed
                        if (num_refined_integration_steps >= num_integration_steps)
                        {
                            this->log_output << "Finished integration for new particles!" << std::endl << std::endl;

                            // Merge positions and output arrays
                            this->positions_forward.insert(this->positions_forward.end(), new_positions_forward.begin(), new_positions_forward.end());
                            this->positions_backward.insert(this->positions_backward.end(), new_positions_backward.begin(), new_positions_backward.end());

                            this->labels_forward.insert(this->labels_forward.end(), new_labels_forward.begin(), new_labels_forward.end());
                            this->labels_backward.insert(this->labels_backward.end(), new_labels_backward.begin(), new_labels_backward.end());

                            this->distances_forward.insert(this->distances_forward.end(), new_distances_forward.begin(), new_distances_forward.end());
                            this->distances_backward.insert(this->distances_backward.end(), new_distances_backward.begin(), new_distances_backward.end());

                            this->terminations_forward.insert(this->terminations_forward.end(), new_terminations_forward.begin(), new_terminations_forward.end());
                            this->terminations_backward.insert(this->terminations_backward.end(), new_terminations_backward.begin(), new_terminations_backward.end());

                            finished_refined_integration = true;
                        }
                    }
                }

                // Set (intermediate) results
                if (finished_refined_integration)
                {
                    set_result(promise, finished || this->terminate_computation);
                }

                // Prepare new results
                if (!finished && finished_refined_integration)
                {
                    std::swap(promise, std::promise<result>());

                    this->current_result = promise.get_future().share();
                }

                // Prevent blocking of CPU
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            this->log_output << "Finished computation!" << (this->terminate_computation ? " (terminated)" : "") << std::endl << std::endl;
        }

        void implicit_topology_computation::set_result(std::promise<result>& promise, const bool finished)
        {
            result current_result;

            auto mesh = this->delaunay.export_grid();
            current_result.vertices = mesh.first;
            current_result.indices = mesh.second;

            current_result.positions_forward = std::make_shared<std::vector<float>>(this->positions_forward);
            current_result.labels_forward = std::make_shared<std::vector<float>>(this->labels_forward);
            current_result.distances_forward = std::make_shared<std::vector<float>>(this->distances_forward);
            current_result.terminations_forward = std::make_shared<std::vector<float>>(this->terminations_forward);

            current_result.positions_backward = std::make_shared<std::vector<float>>(this->positions_backward);
            current_result.labels_backward = std::make_shared<std::vector<float>>(this->labels_backward);
            current_result.distances_backward = std::make_shared<std::vector<float>>(this->distances_backward);
            current_result.terminations_backward = std::make_shared<std::vector<float>>(this->terminations_backward);

            current_result.finished = finished;

            current_result.computation_state.integration_timestep = this->integration_timestep;
            current_result.computation_state.max_integration_error = this->max_integration_error;
            current_result.computation_state.num_integration_steps = this->num_integration_steps_performed;

            promise.set_value(std::move(current_result));
        }

        std::vector<float> implicit_topology_computation::refine_grid(const float refinement_threshold,
            const bool refine_at_labels, const float distance_difference_threshold)
        {
            this->log_output << "Refining grid..." << std::endl;
            this->log_output << "Refinement threshold:                  " << refinement_threshold << std::endl;
            this->log_output << "Refinement at labels:                  " << (refine_at_labels ? "yes" : "no") << std::endl;
            this->log_output << "Distance difference threshold:         " << distance_difference_threshold << std::endl;

            // Find and mark points where the edges should be refined
            std::unordered_set<std::size_t> marked_points;

            std::size_t num_points_by_label = 0;
            std::size_t num_points_by_distance = 0;

            for (auto cell_it = this->delaunay.get_finite_cells_begin(); cell_it != this->delaunay.get_finite_cells_end(); ++cell_it)
            {
                for (int pi = 0; pi < 2; ++pi)
                {
                    const auto vertex_i = cell_it->vertex(pi);
                    const auto point_i = vertex_i->info();

                    const auto label_forward_i = this->labels_forward[point_i];
                    const auto label_backward_i = this->labels_backward[point_i];

                    const auto distance_forward_i = this->distances_forward[point_i];
                    const auto distance_backward_i = this->distances_backward[point_i];

                    const auto termination_forward_i = this->terminations_forward[point_i];
                    const auto termination_backward_i = this->terminations_backward[point_i];

                    for (int pj = pi + 1; pj < 3; ++pj)
                    {
                        const auto vertex_j = cell_it->vertex(pj);
                        const auto point_j = vertex_j->info();

                        const auto label_forward_j = this->labels_forward[point_j];
                        const auto label_backward_j = this->labels_backward[point_j];

                        const auto distance_forward_j = this->distances_forward[point_j];
                        const auto distance_backward_j = this->distances_backward[point_j];

                        const auto termination_forward_j = this->terminations_forward[point_j];
                        const auto termination_backward_j = this->terminations_backward[point_j];

                        if (termination_forward_i == 0 || termination_backward_i == 0 || termination_forward_j == 0 || termination_backward_j == 0)
                        {
                            if (refine_at_labels && (label_forward_i != label_forward_j || label_backward_i != label_backward_j))
                            {
                                ++num_points_by_label;

                                marked_points.insert(point_i);
                                marked_points.insert(point_j);
                            }
                            else if (std::abs(distance_forward_i - distance_forward_j) > distance_difference_threshold
                                || std::abs(distance_backward_i - distance_backward_j) > distance_difference_threshold)
                            {
                                ++num_points_by_distance;

                                marked_points.insert(point_i);
                                marked_points.insert(point_j);
                            }
                        }
                    }
                }
            }

            this->log_output << "Marked points:                         " << marked_points.size() << std::endl;
            this->log_output << "Marked points by label:                " << num_points_by_label << std::endl;
            this->log_output << "Marked points by distance difference:  " << num_points_by_distance << std::endl;

            // Refine edges connected to marked points if they are not too short already
            std::deque<std::pair<triangulation::point_t, std::size_t>> new_points;
            std::size_t point_index = this->delaunay.delaunay.number_of_vertices();

            auto edge_hash = [](const std::pair<std::size_t, std::size_t>& edge) { return std::hash<std::size_t>()(edge.first) ^ std::hash<std::size_t>()(edge.second); };

            std::unordered_set<std::pair<std::size_t, std::size_t>, decltype(edge_hash)> edges(199, edge_hash);
            std::vector<triangulation::point_t> vertices(this->delaunay.delaunay.number_of_vertices());

            for (auto cell_it = this->delaunay.get_finite_cells_begin(); cell_it != this->delaunay.get_finite_cells_end(); ++cell_it)
            {
                for (int pi = 0; pi < 2; ++pi)
                {
                    const auto point_i = static_cast<std::size_t>(cell_it->vertex(pi)->info());

                    for (int pj = pi + 1; pj < 3; ++pj)
                    {
                        const auto point_j = static_cast<std::size_t>(cell_it->vertex(pj)->info());

                        if (marked_points.find(point_i) != marked_points.end() || marked_points.find(point_j) != marked_points.end())
                        {
                            vertices[point_i] = cell_it->vertex(pi)->point();
                            vertices[point_j] = cell_it->vertex(pj)->point();

                            const auto e = std::make_pair(std::min(point_i, point_j), std::max(point_i, point_j));
                            edges.insert(e);
                        }
                    }
                }
            }

            this->log_output << "Marked edges:                          " << edges.size() << std::endl;

            const auto refinement_threshold_squared = refinement_threshold * refinement_threshold;

            for (const auto& edge : edges)
            {
                const auto point_i = edge.first;
                const auto point_j = edge.second;

                const auto edge_point_0 = vertices[point_i];
                const auto edge_point_1 = vertices[point_j];

                const auto sub = edge_point_0 - edge_point_1;
                const auto edge_length_squared = sub.squared_length();

                if (edge_length_squared > refinement_threshold_squared)
                {
                    const auto mid_point = edge_point_1 + 0.5 * sub;

                    new_points.push_back(std::make_pair(mid_point, point_index++));
                }
            }

            if (new_points.size() != 0)
            {
                this->delaunay.delaunay.insert(new_points.begin(), new_points.end());
            }

            this->log_output << "New points:                            " << new_points.size() << std::endl;
            this->log_output << "Refinement finished!" << std::endl;
            this->log_output << std::endl;

            // Return points in usable fashion
            std::vector<float> new_points_gl;
            new_points_gl.reserve(new_points.size());

            std::for_each(new_points.begin(), new_points.end(),
                [&new_points_gl](const std::pair<triangulation::point_t, std::size_t>& value)
            {
                new_points_gl.push_back(static_cast<float>(CGAL::to_double(value.first[0])));
                new_points_gl.push_back(static_cast<float>(CGAL::to_double(value.first[1])));
            });

            return new_points_gl;
        }
    }
}

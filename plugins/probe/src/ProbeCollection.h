/*
 * ProbeCollection.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */


#ifndef PROBE_COLLECTION_H_INCLUDED
#define PROBE_COLLECTION_H_INCLUDED

#include "probe/probe.h"

#include <array>
#include <string>
#include <variant>

namespace megamol {
namespace probe {

struct BaseProbe {
    /** time at which this probes samples the data */
    size_t               m_timestamp;
    /** semantic name of the values/field that this probe samples */
    std::string          m_value_name;
    /** position of probe head on surface */
    std::array<float, 3> m_position;
    /** probe insertion/sampling direction */
    std::array<float, 3> m_direction;
    /** "sample from" offset from position */
    float                m_begin;
    /** "sample to" offset from position */
    float                m_end;
    // std::vector<size_t>m_sample_idxs; ///< indices of samples relevant to this

    virtual void probe() = 0;
};

struct FloatProbe : public BaseProbe {
    void probe() {/* ToDo*/}
};

struct IntProbe : public BaseProbe {
    void probe() { /*ToDo*/}
};

using GenericProbe = std::variant<FloatProbe, IntProbe>;


class ProbeCollection {
public:
    ProbeCollection() = default;
    ~ProbeCollection() = default;

    template <typename ProbeType> void addProbe(ProbeType const& probe) { m_probes.push_back(probe); }

    template <typename ProbeType> ProbeType getProbe(size_t idx) { return std::get<ProbeType>(m_probes[idx]); }

private:

    std::vector<GenericProbe> m_probes;
};


} // namespace probe
} // namespace megamol

#endif // !PROBE_COLLECTION_H_INCLUDED

/*
 * MeshCalls.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#ifndef MESH_CALLS_H_INCLUDED
#define MESH_CALLS_H_INCLUDED

#include "mmcore/CallGeneric.h"

#include <memory>
#include "tiny_gltf.h"

#include "3DInteractionCollection.h"
#include "GPUMaterialCollection.h"
#include "GPUMeshCollection.h"
#include "GPURenderTaskCollection.h"
#include "MeshDataAccessCollection.h"

namespace megamol {
namespace mesh {

/**
 * The most basic meta data only features a hash value to indicate when "something has changed".
 */
struct BasicMetaData {
    size_t m_data_hash = 0;
};

/**
 * Meta data for spatial 3D data communicates the data bounding box as well as frame count
 * and current frame ID for time dependent data.
 * A hash value provides the possibility to communicate when "something has changed".
 */
struct Spatial3DMetaData {
    size_t m_data_hash = 0;
    unsigned int m_frame_cnt = 0;
    unsigned int m_frame_ID = 0;
    megamol::core::BoundingBoxes m_bboxs;
};

class MESH_API Call3DInteraction
    : public core::CallGeneric<std::shared_ptr<ThreeDimensionalInteractionCollection>, BasicMetaData> {
public:
    inline Call3DInteraction() : CallGeneric<std::shared_ptr<ThreeDimensionalInteractionCollection>, BasicMetaData>() {}
    ~Call3DInteraction() = default;

    static const char* ClassName(void) { return "Call3DInteraction"; }
    static const char* Description(void) { return "Call that transports..."; }
};

class MESH_API CallGlTFData : public core::CallGeneric<std::shared_ptr<tinygltf::Model>, BasicMetaData> {
public:
    inline CallGlTFData() : CallGeneric<std::shared_ptr<tinygltf::Model>, BasicMetaData>() {}
    ~CallGlTFData() = default;

    static const char* ClassName(void) { return "CallGlTFData"; }
    static const char* Description(void) { return "Call that gives access to a loaded gltf model."; }
};

class MESH_API CallGPUMaterialData : public core::CallGeneric<std::shared_ptr<GPUMaterialCollecton>, BasicMetaData> {
public:
    CallGPUMaterialData() : CallGeneric<std::shared_ptr<GPUMaterialCollecton>, BasicMetaData>() {}
    ~CallGPUMaterialData() = default;

    static const char* ClassName(void) { return "CallGPUMaterialData"; }
    static const char* Description(void) { return "Call that gives access to material data stored on the GPU."; }
};

class MESH_API CallGPUMeshData : public core::CallGeneric<std::shared_ptr<GPUMeshCollection>, Spatial3DMetaData> {
public:
    CallGPUMeshData() : CallGeneric<std::shared_ptr<GPUMeshCollection>, Spatial3DMetaData>() {}
    ~CallGPUMeshData() = default;

    static const char* ClassName(void) { return "CallGPUMeshData"; }
    static const char* Description(void) {
        return "Call that gives access to meshes stored in batches on the GPU for rendering.";
    }
};

class MESH_API CallGPURenderTaskData
    : public core::CallGeneric<std::shared_ptr<GPURenderTaskCollection>, Spatial3DMetaData> {
public:
    CallGPURenderTaskData() : CallGeneric<std::shared_ptr<GPURenderTaskCollection>, Spatial3DMetaData>() {}
    ~CallGPURenderTaskData(){};

    static const char* ClassName(void) { return "CallGPURenderTaskData"; }
    static const char* Description(void) { return "Call that gives access to render tasks."; }
};

class MESH_API CallMesh : public core::CallGeneric<std::shared_ptr<MeshDataAccessCollection>, Spatial3DMetaData> {
public:
    CallMesh() : CallGeneric<std::shared_ptr<MeshDataAccessCollection>, Spatial3DMetaData>() {}
    ~CallMesh(){};

    static const char* ClassName(void) { return "CallMesh"; }
    static const char* Description(void) { return "Call that gives access to CPU-side mesh data."; }
};

/** Description class typedef */
typedef megamol::core::factories::CallAutoDescription<CallGPURenderTaskData> GPURenderTasksDataCallDescription;
typedef megamol::core::factories::CallAutoDescription<CallGPUMeshData> CallGPUMeshDataDescription;
typedef megamol::core::factories::CallAutoDescription<CallGPUMaterialData> CallGPUMaterialDataDescription;
typedef megamol::core::factories::CallAutoDescription<Call3DInteraction> Call3DInteractionDescription;
typedef megamol::core::factories::CallAutoDescription<CallGlTFData> CallGlTFDataDescription;

} // namespace mesh
} // namespace megamol


#endif // !MESH_CALLS_H_INCLUDED

/*
 * AddClusterColours.cpp
 *
 * Copyright (C) 2009 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "AddClusterColours.h"
#include "geometry_calls/MultiParticleDataCall.h"
#include "mmcore/param/ButtonParam.h"
#include "mmstd_gl/renderer/CallGetTransferFunctionGL.h"
#include "vislib/RawStorage.h"
#include "vislib/RawStorageWriter.h"
#include "vislib_gl/graphics/gl/IncludeAllGL.h"

namespace megamol::datatools_gl::misc {

/****************************************************************************/

/*
 * AddClusterColours::Unlocker::Unlocker
 */
AddClusterColours::Unlocker::Unlocker(geocalls::MultiParticleDataCall::Unlocker* inner)
        : geocalls::MultiParticleDataCall::Unlocker()
        , inner(inner) {
    // intentionally empty ATM
}


/*
 * AddClusterColours::Unlocker::~Unlocker
 */
AddClusterColours::Unlocker::~Unlocker() {
    this->Unlock();
}


/*
 * AddClusterColours::Unlocker::Unlock
 */
void AddClusterColours::Unlocker::Unlock() {
    if (this->inner != NULL) {
        this->inner->Unlock();
        SAFE_DELETE(this->inner);
    }
}


/****************************************************************************/


/*
 * AddClusterColours::AddClusterColours
 */
AddClusterColours::AddClusterColours()
        : Module()
        , putDataSlot("putdata", "Connects from the data consumer")
        , getDataSlot("getdata", "Connects to the data source")
        , getTFSlot("gettransferfunction", "Connects to the transfer function module")
        , rebuildButtonSlot("rebuild", "Forces rebuild of colour data")
        , lastFrame(0)
        , colData()
        , updateHash() {

    this->putDataSlot.SetCallback("MultiParticleDataCall", "GetData", &AddClusterColours::getDataCallback);
    this->putDataSlot.SetCallback("MultiParticleDataCall", "GetExtent", &AddClusterColours::getExtentCallback);
    this->MakeSlotAvailable(&this->putDataSlot);

    this->getDataSlot.SetCompatibleCall<geocalls::MultiParticleDataCallDescription>();
    this->MakeSlotAvailable(&this->getDataSlot);

    this->getTFSlot.SetCompatibleCall<mmstd_gl::CallGetTransferFunctionGLDescription>();
    this->MakeSlotAvailable(&this->getTFSlot);

    this->rebuildButtonSlot << new core::param::ButtonParam();
    this->MakeSlotAvailable(&this->rebuildButtonSlot);
}


/*
 * AddClusterColours::~AddClusterColours
 */
AddClusterColours::~AddClusterColours() {
    this->Release();
}


/*
 * AddClusterColours::create
 */
bool AddClusterColours::create() {
    return true;
}


/*
 * AddClusterColours::release
 */
void AddClusterColours::release() {
    // intentionally empty ATM
}


/*
 * AddClusterColours::getDataCallback
 */
bool AddClusterColours::getDataCallback(core::Call& caller) {
    static vislib::RawStorage updateHash;
    static vislib::RawStorageWriter uhWriter(updateHash);
    geocalls::MultiParticleDataCall* inCall = dynamic_cast<geocalls::MultiParticleDataCall*>(&caller);
    if (inCall == NULL)
        return false;

    geocalls::MultiParticleDataCall* outCall = this->getDataSlot.CallAs<geocalls::MultiParticleDataCall>();
    if (outCall == NULL)
        return false;

    *outCall = *inCall;

    if ((*outCall)(0)) {
        uhWriter.SetPosition(0);

        SIZE_T cntCol = 0;
        for (unsigned int i = 0; i < outCall->GetParticleListCount(); i++) {
            geocalls::MultiParticleDataCall::Particles& part = outCall->AccessParticles(i);
            if (part.GetColourDataType() == geocalls::MultiParticleDataCall::Particles::COLDATA_FLOAT_I) {
                SIZE_T cnt = static_cast<SIZE_T>(part.GetCount());
                cntCol += cnt;
                uhWriter.Write(part.GetMinColourIndexValue());
                uhWriter.Write(part.GetMaxColourIndexValue());
                const float* cd = static_cast<const float*>(part.GetColourData());
                uhWriter.Write(cd[0]);
                uhWriter.Write(cd[(cnt - 1) / 2]);
                uhWriter.Write(cd[cnt - 1]);
            }
        }

        if (cntCol > 0) {
            if ((this->lastFrame != outCall->FrameID()) || ((cntCol * 3) != this->colData.GetSize()) ||
                this->rebuildButtonSlot.IsDirty() || (uhWriter.End() != this->updateHash.GetSize()) ||
                (::memcmp(updateHash.As<void>(), this->updateHash.As<void>(), uhWriter.End()) != 0)) {

                this->updateHash = updateHash;
                this->updateHash.EnforceSize(uhWriter.End(), true);
                this->rebuildButtonSlot.ResetDirty();

                // build colour data
                this->lastFrame = outCall->FrameID();
                this->colData.EnforceSize(cntCol * 3);

                vislib::RawStorage texDat;

                mmstd_gl::CallGetTransferFunctionGL* cgtf =
                    this->getTFSlot.CallAs<mmstd_gl::CallGetTransferFunctionGL>();
                if ((cgtf != NULL) && ((*cgtf)(0))) {
                    ::glGetError();
                    ::glEnable(GL_TEXTURE_1D);
                    ::glBindTexture(GL_TEXTURE_1D, cgtf->OpenGLTexture());

                    int texSize = 0;
                    ::glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &texSize);

                    if (::glGetError() == GL_NO_ERROR) {
                        texDat.EnforceSize(texSize * 12);
                        ::glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_FLOAT, texDat.As<void>());
                        if (::glGetError() != GL_NO_ERROR) {
                            texDat.EnforceSize(0);
                        }
                    }

                    ::glBindTexture(GL_TEXTURE_1D, 0);
                    ::glDisable(GL_TEXTURE_1D);
                }

                unsigned int texDatSize = 2;
                if (texDat.GetSize() < 24) {
                    texDat.EnforceSize(24);
                    *texDat.AsAt<float>(0) = 0.0f;
                    *texDat.AsAt<float>(4) = 0.0f;
                    *texDat.AsAt<float>(8) = 0.0f;
                    *texDat.AsAt<float>(12) = 1.0f;
                    *texDat.AsAt<float>(16) = 1.0f;
                    *texDat.AsAt<float>(20) = 1.0f;
                } else {
                    texDatSize = static_cast<unsigned int>(texDat.GetSize() / 12);
                }
                texDatSize--;

                cntCol = 0;
                for (unsigned int i = 0; i < outCall->GetParticleListCount(); i++) {
                    geocalls::MultiParticleDataCall::Particles& parts = outCall->AccessParticles(i);
                    if (parts.GetColourDataType() != geocalls::MultiParticleDataCall::Particles::COLDATA_FLOAT_I)
                        continue;
                    const float* values = static_cast<const float*>(parts.GetColourData());
                    for (UINT64 j = 0; j < parts.GetCount(); j++) {
                        //                        float v = (values[j] - parts.GetMinColourIndexValue()) /
                        //                            (parts.GetMaxColourIndexValue() - parts.GetMinColourIndexValue());
                        unsigned int clusterId = (unsigned int)(values[j] / 100000000.0);
                        unsigned int colorIdx = (clusterId % 13);
                        float v = static_cast<float>(colorIdx) / 13.0f;
                        if (v < 0.0f)
                            v = 0.0f;
                        else if (v > 1.0f)
                            v = 1.0f;

                        v *= static_cast<float>(texDatSize);

                        int idx = static_cast<int>(v);
                        v -= static_cast<float>(idx);
                        float w = 1.0f - v;
                        if (idx == static_cast<int>(texDatSize)) {
                            idx--;
                            v = 1.0f;
                            w = 0.0;
                        }

                        *this->colData.AsAt<BYTE>(cntCol++) = static_cast<BYTE>(
                            255.0f * (*texDat.AsAt<float>(idx * 12) * w + *texDat.AsAt<float>(idx * 12 + 12) * v));
                        *this->colData.AsAt<BYTE>(cntCol++) = static_cast<BYTE>(
                            255.0f * (*texDat.AsAt<float>(idx * 12 + 4) * w + *texDat.AsAt<float>(idx * 12 + 16) * v));
                        *this->colData.AsAt<BYTE>(cntCol++) = static_cast<BYTE>(
                            255.0f * (*texDat.AsAt<float>(idx * 12 + 8) * w + *texDat.AsAt<float>(idx * 12 + 20) * v));
                    }
                }
            }

            *inCall = *outCall;
            outCall->SetUnlocker(NULL, false);
            inCall->SetUnlocker(new Unlocker(inCall->GetUnlocker()), false);

            cntCol = 0;
            for (unsigned int i = 0; i < inCall->GetParticleListCount(); i++) {
                if (inCall->AccessParticles(i).GetColourDataType() ==
                    geocalls::MultiParticleDataCall::Particles::COLDATA_FLOAT_I) {
                    inCall->AccessParticles(i).SetColourData(
                        geocalls::MultiParticleDataCall::Particles::COLDATA_UINT8_RGB, this->colData.At(cntCol));
                    cntCol += 3 * static_cast<SIZE_T>(inCall->AccessParticles(i).GetCount());
                }
            }

            return true;

        } else {
            *inCall = *outCall;
            outCall->SetUnlocker(NULL, false);
            return true;
        }
    }

    return false;
}


/*
 * AddClusterColours::getExtentCallback
 */
bool AddClusterColours::getExtentCallback(core::Call& caller) {
    geocalls::MultiParticleDataCall* inCall = dynamic_cast<geocalls::MultiParticleDataCall*>(&caller);
    if (inCall == NULL)
        return false;

    geocalls::MultiParticleDataCall* outCall = this->getDataSlot.CallAs<geocalls::MultiParticleDataCall>();
    if (outCall == NULL)
        return false;

    *outCall = *inCall;

    if ((*outCall)(1)) {
        outCall->SetUnlocker(NULL, false);
        *inCall = *outCall;
        return true;
    }

    return false;
}
} // namespace megamol::datatools_gl::misc

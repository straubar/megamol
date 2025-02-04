#include "FBOTransmitter2.h"

#include <array>

#include "glad/glad.h"

#include "snappy.h"

#include "mmcore/utility/log/Log.h"

#include "cluster/mpi/MpiCall.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/view/CallRender2DGL.h"
#include "mmcore/view/CallRender3DGL.h"
#include "vislib/Trace.h"
#include "vislib/sys/SystemInformation.h"

#ifdef __unix__
#include <limits.h>
#include <unistd.h>
#endif

//#define _DEBUG 1

megamol::remote::FBOTransmitter2::FBOTransmitter2()
        : address_slot_{"port", "The port the transmitter should connect to"}
        , commSelectSlot_{"communicator", "Select the communicator to use"}
        , view_name_slot_{"view", "The name of the view instance to be used (required)"}
        , trigger_button_slot_{"trigger", "Triggers transmission"}
        , target_machine_slot_{"targetMachine", "Name of the target machine"}
        , force_localhost_slot_{"force_localhost", "Enable to enforce localhost as hostname for handshake"}
        , handshake_port_slot_{"handshakePort", "Port for zmq handshake"}
        , reconnect_slot_{"reconnect", "Reconnect comm threads"}
        , tiled_slot_("tiledDisplay", "True if rendering on a tiled display")
#ifdef MEGAMOL_USE_MPI
        , callRequestMpi("requestMpi", "Requests initialisation of MPI and the communicator for the view.")
        , toggle_aggregate_slot_{"aggregate", "Toggle whether to aggregate and composite FBOs prior to transmission"}
        , render_comp_img_slot_("renderCompImage", "Renders the complete composited image on the broadcast master")
#endif // MEGAMOL_USE_MPI
        , aggregate_{false}
        , frame_id_{0}
        , thread_stop_{false}
        , fbo_msg_read_{new fbo_msg_header_t}
        , fbo_msg_send_{new fbo_msg_header_t}
        , color_buf_read_{new std::vector<char>}
        , depth_buf_read_{new std::vector<char>}
        , color_buf_send_{new std::vector<char>}
        , depth_buf_send_{new std::vector<char>}
        , col_buf_el_size_{4}
        , depth_buf_el_size_{4}
        , connected_{false}
        , validViewport(false) {
    this->address_slot_ << new megamol::core::param::StringParam{"34242"};
    this->MakeSlotAvailable(&this->address_slot_);
    this->handshake_port_slot_ << new megamol::core::param::IntParam(42000);
    this->MakeSlotAvailable(&this->handshake_port_slot_);
    auto ep = new megamol::core::param::EnumParam(FBOCommFabric::ZMQ_COMM);
    ep->SetTypePair(FBOCommFabric::ZMQ_COMM, "ZMQ");
    ep->SetTypePair(FBOCommFabric::MPI_COMM, "MPI");
    commSelectSlot_ << ep;
    this->MakeSlotAvailable(&commSelectSlot_);
    this->view_name_slot_ << new megamol::core::param::StringParam{"::inst::view"};
    this->MakeSlotAvailable(&this->view_name_slot_);
    this->trigger_button_slot_ << new megamol::core::param::ButtonParam{
        core::view::Key::KEY_T, core::view::Modifier::ALT};
    this->trigger_button_slot_.SetUpdateCallback(&FBOTransmitter2::triggerButtonClicked);
    this->MakeSlotAvailable(&this->trigger_button_slot_);
    this->target_machine_slot_ << new megamol::core::param::StringParam{"127.0.0.1"};
    this->MakeSlotAvailable(&this->target_machine_slot_);
    this->force_localhost_slot_ << new megamol::core::param::BoolParam{false};
    this->MakeSlotAvailable(&this->force_localhost_slot_);
#ifdef MEGAMOL_USE_MPI
    this->callRequestMpi.SetCompatibleCall<core::cluster::mpi::MpiCallDescription>();
    this->MakeSlotAvailable(&this->callRequestMpi);
    toggle_aggregate_slot_ << new megamol::core::param::BoolParam{false};
    this->MakeSlotAvailable(&toggle_aggregate_slot_);
    render_comp_img_slot_ << new megamol::core::param::BoolParam{false};
    this->render_comp_img_slot_.SetUpdateCallback(&FBOTransmitter2::renderCompChanged);
    this->MakeSlotAvailable(&render_comp_img_slot_);
#endif // MEGAMOL_USE_MPI
    reconnect_slot_ << new megamol::core::param::ButtonParam{};
    reconnect_slot_.SetUpdateCallback(&FBOTransmitter2::reconnectCallback);
    this->MakeSlotAvailable(&reconnect_slot_);

    tiled_slot_ << new megamol::core::param::BoolParam(false);
    this->MakeSlotAvailable(&tiled_slot_);
}


megamol::remote::FBOTransmitter2::~FBOTransmitter2() {
    this->Release();
}


bool megamol::remote::FBOTransmitter2::create() {
#if _DEBUG
    megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Creating ...\n");
#endif
    return true;
}


void megamol::remote::FBOTransmitter2::release() {
    shutdownThreads();
}


void megamol::remote::FBOTransmitter2::AfterRender(megamol::core::view::AbstractView* view) {

#ifdef MEGAMOL_USE_MPI
    if (!this->render_comp_img_slot_.Param<core::param::BoolParam>()->Value()) {
        initThreads();
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: initThreads ... Done");
#endif
    }
#else
    initThreads();
#endif

    if (!this->validViewport) {
        if (!this->tiled_slot_.Param<core::param::BoolParam>()->Value() || !this->extractViewport(this->viewport)) {
            GLint glvp[4];
            glGetIntegerv(GL_VIEWPORT, glvp);
            for (int i = 0; i < 4; ++i) {
                this->viewport[i] = glvp[i];
            }
            this->viewport[4] = glvp[2];
            this->viewport[5] = glvp[3];
        }
    }
    int xoff = this->viewport[0];
    int yoff = this->viewport[1];
    int tile_width = this->viewport[2];
    int tile_height = this->viewport[3];
    int width = this->viewport[4];
    int height = this->viewport[5];

#if _DEBUG
    megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Extracting Viewport ... Done");
#endif

    // read FBO
    std::vector<char> col_buf(width * height * col_buf_el_size_);
    std::vector<char> depth_buf(width * height * depth_buf_el_size_);

    if ((tile_width == width) && (tile_height == height)) {
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, col_buf.data());
        glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth_buf.data());
    } else {
        std::vector<char> col_buf_tile(tile_width * tile_height * col_buf_el_size_);
        std::vector<char> depth_buf_tile(tile_width * tile_height * depth_buf_el_size_);

        glReadPixels(0, 0, tile_width, tile_height, GL_RGBA, GL_UNSIGNED_BYTE, col_buf_tile.data());
        glReadPixels(0, 0, tile_width, tile_height, GL_DEPTH_COMPONENT, GL_FLOAT, depth_buf_tile.data());

        int row_offset = yoff * width; // y * width = row offset * tile width
        int column_offset = xoff;      // x  = column offset
        int color_row_tile_width = col_buf_el_size_ * tile_width;
        int depth_row_tile_width = depth_buf_el_size_ * tile_width;

        // Copy tile rows to right position to fit row major format
        int offset = 0;
        for (int i = 0; i < tile_height; ++i) {
            offset = row_offset + column_offset + (i * width);
            memcpy(col_buf.data() + (col_buf_el_size_ * offset), col_buf_tile.data() + (i * color_row_tile_width),
                color_row_tile_width);
            memcpy(depth_buf.data() + (depth_buf_el_size_ * offset), depth_buf_tile.data() + (i * depth_row_tile_width),
                depth_row_tile_width);
        }
    }

#if _DEBUG
    megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: readFBO ... Done");
#endif


#ifdef MEGAMOL_USE_MPI
    IceTUByte* icet_col_buf = reinterpret_cast<IceTUByte*>(col_buf.data());
    IceTFloat* icet_depth_buf = reinterpret_cast<IceTFloat*>(depth_buf.data());

    if (aggregate_) {
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo(
            "FBOTransmitter2: Simple IceT commit at rank %d\n", mpiRank);
#endif
        std::array<float, 4> backgroundColor = {0.0f, 0.0f, 0.0f, 1.0f};
        this->extractBackgroundColor(backgroundColor);

        int tilevp[4] = {xoff, yoff, tile_width, tile_height}; // define current valid pixel viewport for icet
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo(
            "IceT gets image with xoff: %d, yoff: %d, tile_width: %d, tile_height: %d\n", xoff, yoff, tile_width,
            tile_height);
#endif
        auto const icet_comp_image = icetCompositeImage(col_buf.data(), depth_buf.data(), tilevp, nullptr, nullptr,
            static_cast<const IceTFloat*>(backgroundColor.data()));
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: IceT - Composite Image Done\n");
#endif

        if (mpiRank == 0) {
            icet_col_buf = icetImageGetColorub(icet_comp_image);
            icet_depth_buf = icetImageGetDepthf(icet_comp_image);
#if _DEBUG
            megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: IceT - ImageGet Done\n");
#endif
            if (this->render_comp_img_slot_.Param<core::param::BoolParam>()->Value()) {
                glDrawPixels(tile_width, tile_height, GL_RGBA, GL_UNSIGNED_BYTE, icet_col_buf);
            }
        }
    }

    if ((aggregate_ && mpiRank == 0) || !aggregate_) {
#endif // MEGAMOL_USE_MPI


#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Extracting Meta Data ...\n");
#endif
        // extract meta data
        float times[2] = {0.0f, 0.0f};
        float bbox[6] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
        float camera[9] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f};
        if (!this->extractMetaData(bbox, times, camera)) {
            megamol::core::utility::log::Log::DefaultLog.WriteError("FBOTransmitter2: Could not extract meta data.\n");
        }
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Extracting Meta Data ... Done\n");
#endif
        // copy data to read buffer, if possible
        {
            std::lock_guard<std::mutex> read_guard{this->buffer_read_guard_}; //< maybe try_lock instead

#if _DEBUG
            megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Swapping Buffer ...\n");
#endif

            int vp[4] = {0, 0, width, height}; // full viewport is needed here
            for (int i = 0; i < 4; ++i) {
                this->fbo_msg_read_->screen_area[i] = this->fbo_msg_read_->updated_area[i] = vp[i];
            }
            this->fbo_msg_read_->color_type = fbo_color_type::RGBAu8;
            this->fbo_msg_read_->depth_type = fbo_depth_type::Df;
            for (int i = 0; i < 6; ++i) {
                this->fbo_msg_read_->os_bbox[i] = this->fbo_msg_read_->cs_bbox[i] = bbox[i];
            }
            for (int i = 0; i < 2; ++i) {
                this->fbo_msg_read_->frame_times[i] = times[i];
            }
            for (int i = 0; i < 9; ++i) {
                this->fbo_msg_read_->cam_params[i] = camera[i];
            }

            this->color_buf_read_->resize(col_buf.size());
            this->depth_buf_read_->resize(depth_buf.size());

#ifdef MEGAMOL_USE_MPI
            // std::copy(col_buf.begin(),   col_buf.end(),   this->color_buf_read_->begin());
            memcpy(this->color_buf_read_->data(), icet_col_buf, width * height * col_buf_el_size_);
            // std::copy(depth_buf.begin(), depth_buf.end(), this->depth_buf_read_->begin());
            memcpy(this->depth_buf_read_->data(), icet_depth_buf, width * height * depth_buf_el_size_);
#else
        std::copy(col_buf.begin(), col_buf.end(), this->color_buf_read_->begin());
        std::copy(depth_buf.begin(), depth_buf.end(), this->depth_buf_read_->begin());
#endif // MEGAMOL_USE_MPI

            this->fbo_msg_read_->frame_id = this->frame_id_.fetch_add(1);
        }

        this->swapBuffers();
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Swapping Buffer ... Done\n");
#endif

#ifdef MEGAMOL_USE_MPI
    }
#endif // MEGAMOL_USE_MPI
}


void megamol::remote::FBOTransmitter2::transmitterJob() {
    try {
        while (!this->thread_stop_) {
            // transmit only upon request
            std::vector<char> buf;
            try {
#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Waiting for request\n");
#endif
                /*if (!this->comm_->Recv(buf, recv_type::RECV)) {
                    megamol::core::utility::log::Log::DefaultLog.WriteError("FBOTransmitter2: Error during recv in 'transmitterJob'\n");
                }*/
                while (!this->comm_->Recv(buf, recv_type::RECV) && !this->thread_stop_) {
#if _DEBUG
                    megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                        "FBOTransmitter2: Recv failed in 'transmitterJob' trying again\n");
#endif
                }
                /*#if _DEBUG
                                else {
                                    megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Request received\n");
                                }
                #endif*/
            } catch (zmq::error_t const& e) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "FBOTransmitter2: Exception during recv in 'transmitterJob': %s\n", e.what());
            } catch (...) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "FBOTransmitter2: Exception during recv in 'transmitterJob'\n");
            }

            // wait for request
            {
                std::lock_guard<std::mutex> send_lock(this->buffer_send_guard_);

                //#ifdef MEGAMOL_USE_MPI
                //            IceTUByte* icet_col_buf = nullptr;
                //            IceTFloat* icet_depth_buf = nullptr;
                //            if (aggregate_) {
                //#if _DEBUG
                //                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Complex IceT commit at rank
                //                %d\n", rank_);
                //#endif
                //                std::array<IceTFloat, 4> backgroundColor = {0, 0, 0, 0};
                //                auto const icet_comp_image = icetCompositeImage(this->color_buf_send_->data(),
                //                    this->depth_buf_send_->data(), nullptr, nullptr, nullptr, backgroundColor.data());
                //#if _DEBUG
                //                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Recieved IceT image at rank
                //                %d\n", rank_);
                //#endif
                //                icet_col_buf = icetImageGetColorub(icet_comp_image);
                //                icet_depth_buf = icetImageGetDepthf(icet_comp_image);
                //            }
                //#endif

                // snappy compression
                std::vector<char> col_comp_buf(snappy::MaxCompressedLength(this->color_buf_send_->size()));
                std::vector<char> depth_comp_buf(snappy::MaxCompressedLength(this->depth_buf_send_->size()));
                size_t col_comp_size = 0;
                size_t depth_comp_size = 0;

                // if (aggregate_) {
                //    snappy::RawCompress(reinterpret_cast<char*>(icet_col_buf), this->color_buf_send_->size(),
                //        col_comp_buf.data(), &col_comp_size);
                //    snappy::RawCompress(reinterpret_cast<char*>(icet_depth_buf), this->depth_buf_send_->size(),
                //        depth_comp_buf.data(), &depth_comp_size);
                //} else {
                snappy::RawCompress(
                    this->color_buf_send_->data(), this->color_buf_send_->size(), col_comp_buf.data(), &col_comp_size);
                snappy::RawCompress(this->depth_buf_send_->data(), this->depth_buf_send_->size(), depth_comp_buf.data(),
                    &depth_comp_size);
                //}

                fbo_msg_send_->color_buf_size = col_comp_size;
                fbo_msg_send_->depth_buf_size = depth_comp_size;
                // compose message from header, color_buf, and depth_buf
                buf.resize(sizeof(fbo_msg_header_t) + col_comp_size + depth_comp_size);
                std::copy(reinterpret_cast<char*>(&(*fbo_msg_send_)),
                    reinterpret_cast<char*>(&(*fbo_msg_send_)) + sizeof(fbo_msg_header_t), buf.data());
                /*std::copy(
                    this->color_buf_send_->begin(), this->color_buf_send_->end(), buf.data() +
                sizeof(fbo_msg_header_t)); std::copy(this->depth_buf_send_->begin(), this->depth_buf_send_->end(),
                    buf.data() + sizeof(fbo_msg_header_t) + this->color_buf_send_->size());*/
                std::copy(
                    col_comp_buf.data(), col_comp_buf.data() + col_comp_size, buf.data() + sizeof(fbo_msg_header_t));
                std::copy(depth_comp_buf.data(), depth_comp_buf.data() + depth_comp_size,
                    buf.data() + sizeof(fbo_msg_header_t) + col_comp_size);

                // send data
                try {
#if _DEBUG
                    megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Sending answer\n");
#endif
                    if (!this->comm_->Send(buf, send_type::SEND)) {
                        megamol::core::utility::log::Log::DefaultLog.WriteError(
                            "FBOTransmitter2: Error during send in 'transmitterJob'\n");
                    }
#if _DEBUG
                    else {
                        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Answer sent\n");
                    }
#endif
                } catch (zmq::error_t const& e) {
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "FBOTransmitter2: Exception during send in 'transmitterJob': %s\n", e.what());
                } catch (...) {
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "FBOTransmitter2: Exception during send in 'transmitterJob'\n");
                }
            }
        }
    } catch (...) {
        megamol::core::utility::log::Log::DefaultLog.WriteError("FBOTransmitter2: TransmitterJob died\n");
    }
}


bool megamol::remote::FBOTransmitter2::triggerButtonClicked(megamol::core::param::ParamSlot& slot) {
    // happy trigger finger hit button action happened
    using megamol::core::utility::log::Log;

#ifdef MEGAMOL_USE_MPI
    initIceT();
#endif

    bool success = true;
    std::string mvn(view_name_slot_.Param<megamol::core::param::StringParam>()->Value());

    Log::DefaultLog.WriteInfo("Transmission of \"%s\" requested", mvn.c_str());

    // this->ModuleGraphLock().LockExclusive();
    const auto ret = this->GetCoreInstance()->FindModuleNoLock<megamol::core::view::AbstractView>(
        mvn, [this](megamol::core::view::AbstractView* vi) { vi->RegisterHook(this); });
    if (!ret) {
        Log::DefaultLog.WriteError("FBOTransmitter2: Unable to find VIEW \"%s\" for transmission", mvn.c_str());
        success = false;
    }
    // this->ModuleGraphLock().UnlockExclusive();

    return true;
}


bool megamol::remote::FBOTransmitter2::extractMetaData(float bbox[6], float frame_times[2], float cam_params[9]) {
    using megamol::core::utility::log::Log;

    bool success = true;
    std::string mvn(view_name_slot_.Param<megamol::core::param::StringParam>()->Value());

    // this->ModuleGraphLock().LockExclusive();
    const auto retBbox =
        this->GetCoreInstance()
            ->EnumerateCallerSlotsNoLock<megamol::core::view::AbstractView, megamol::core::view::CallRender3DGL>(
                mvn, [bbox](megamol::core::view::CallRender3DGL& cr3d) {
                    bbox[0] = cr3d.AccessBoundingBoxes().BoundingBox().GetLeft();
                    bbox[1] = cr3d.AccessBoundingBoxes().BoundingBox().GetBottom();
                    bbox[2] = cr3d.AccessBoundingBoxes().BoundingBox().GetBack();
                    bbox[3] = cr3d.AccessBoundingBoxes().BoundingBox().GetRight();
                    bbox[4] = cr3d.AccessBoundingBoxes().BoundingBox().GetTop();
                    bbox[5] = cr3d.AccessBoundingBoxes().BoundingBox().GetFront();
                });

    const auto retTimes =
        this->GetCoreInstance()
            ->EnumerateCallerSlotsNoLock<megamol::core::view::AbstractView, megamol::core::view::CallRender3DGL>(
                mvn, [frame_times](megamol::core::view::CallRender3DGL& cr3d) {
                    frame_times[0] = cr3d.Time();
                    frame_times[1] = static_cast<float>(cr3d.TimeFramesCount());
                });

    const auto retCam =
        this->GetCoreInstance()
            ->EnumerateCallerSlotsNoLock<megamol::core::view::AbstractView, megamol::core::view::CallRender3DGL>(
                mvn, [cam_params](megamol::core::view::CallRender3DGL& cr3d) {
                    core::view::Camera cam = cr3d.GetCamera();
                    auto cam_pose = cam.get<core::view::Camera::Pose>();
                    auto pos = cam_pose.position;
                    auto up = cam_pose.up;
                    auto view = cam_pose.direction;
                    cam_params[0] = pos.x;
                    cam_params[1] = pos.y;
                    cam_params[2] = pos.z;
                    cam_params[3] = up.x;
                    cam_params[4] = up.y;
                    cam_params[5] = up.z;
                    cam_params[6] = view.x;
                    cam_params[7] = view.y;
                    cam_params[8] = view.z;
                });

    if (!(retBbox && retTimes && retCam)) {
        if (!mvn.empty()) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Unable to find VIEW \"%s\" to extract meta data.\n");
        } else {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Could not find VIEW with empty name.\n");
        }
        success = false;
    }
    // this->ModuleGraphLock().UnlockExclusive();

    return success;
}


bool megamol::remote::FBOTransmitter2::extractViewport(int vvpt[6]) {
    using megamol::core::utility::log::Log;

    bool success = true;
    std::string mvn(view_name_slot_.Param<megamol::core::param::StringParam>()->Value());

    // this->ModuleGraphLock().LockExclusive();
    auto const ret =
        this->GetCoreInstance()
            ->EnumerateCallerSlotsNoLock<megamol::core::view::AbstractView, megamol::core::view::CallRender3DGL>(
                mvn, [vvpt](megamol::core::view::CallRender3DGL& cr3d) {
                    core::view::Camera_2 cam;
                    cr3d.GetCamera(cam);
                    auto tile_rect = cam.image_tile();
                    auto res_gate = cam.resolution_gate();
                    vvpt[0] = static_cast<int>(tile_rect.left());
                    vvpt[1] = static_cast<int>(tile_rect.bottom());
                    vvpt[2] = static_cast<int>(tile_rect.width());
                    vvpt[3] = static_cast<int>(tile_rect.height());
                    vvpt[4] = static_cast<int>(res_gate.width());
                    vvpt[5] = static_cast<int>(res_gate.height());
                });

    if (!ret) {
        if (!mvn.empty()) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Unable to find VIEW \"%s\" to extract viewport.\n");
        } else {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Could not find VIEW with empty name.\n");
        }
        success = false;
    }
    // this->ModuleGraphLock().UnlockExclusive();

    return success;
}


bool megamol::remote::FBOTransmitter2::extractBackgroundColor(std::array<float, 4>& bg_color) {
    using megamol::core::utility::log::Log;

    bool success = true;
    std::string mvn(view_name_slot_.Param<megamol::core::param::StringParam>()->Value());

    // this->ModuleGraphLock().LockExclusive();
    const auto ret = this->GetCoreInstance()->FindModuleNoLock<core::view::AbstractView>(
        mvn, [&bg_color](core::view::AbstractView* arv) {
            auto bgCol = arv->BackgroundColor();
            if (bgCol != glm::vec4(0, 0, 0, 0)) {
                bg_color[0] = bgCol[0];
                bg_color[1] = bgCol[1];
                bg_color[2] = bgCol[2];
                bg_color[3] = 1.0f;
            }
        });

    if (!ret) {
        if (!mvn.empty()) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Unable to find VIEW \"%s\" to extract background color.\n");
        } else {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "FBOTransmitter2: Could not find VIEW with empty name.\n");
        }
        success = false;
    }
    // this->ModuleGraphLock().UnlockExclusive();

    return success;
}


bool megamol::remote::FBOTransmitter2::initMPI() {
    bool retval = false;
#ifdef MEGAMOL_USE_MPI
    if (this->mpi_comm_ == MPI_COMM_NULL) {
        VLTRACE(vislib::Trace::LEVEL_INFO, "FBOTransmitter2: Need to initialize MPI\n");
        auto c = this->callRequestMpi.CallAs<core::cluster::mpi::MpiCall>();
        if (c != nullptr) {
            /* New method: let MpiProvider do all the stuff. */
            if ((*c)(core::cluster::mpi::MpiCall::IDX_PROVIDE_MPI)) {
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter: Got MPI communicator.");
                this->mpi_comm_ = c->GetComm();
            } else {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    _T("FBOTransmitter: Could not ")
                    _T("retrieve MPI communicator for the MPI-based view ")
                    _T("from the registered provider module."));
            }
        }

        if (this->mpi_comm_ != MPI_COMM_NULL) {
            megamol::core::utility::log::Log::DefaultLog.WriteInfo(_T("FBOTransmitter: MPI is ready, ")
                                                                   _T("retrieving communicator properties ..."));
            ::MPI_Comm_rank(this->mpi_comm_, &this->mpiRank);
            ::MPI_Comm_size(this->mpi_comm_, &this->mpiSize);
            megamol::core::utility::log::Log::DefaultLog.WriteInfo(_T("FBOTransmitter on %hs is %d ")
                                                                   _T("of %d."),
                vislib::sys::SystemInformation::ComputerNameA().PeekBuffer(), this->mpiRank, this->mpiSize);
        } /* end if (this->comm != MPI_COMM_NULL) */
        VLTRACE(vislib::Trace::LEVEL_INFO, "FBOTransmitter2: MPI initialized: %s (%i)\n",
            this->mpi_comm_ != MPI_COMM_NULL ? "true" : "false", mpi_comm_);
    } /* end if (this->comm == MPI_COMM_NULL) */

    /* Determine success of the whole operation. */
    retval = (this->mpi_comm_ != MPI_COMM_NULL);
#endif /* MEGAMOL_USE_MPI */
    return retval;
}


bool megamol::remote::FBOTransmitter2::reconnectCallback(megamol::core::param::ParamSlot& p) {
    shutdownThreads();
    initThreads();

    return true;
}


bool megamol::remote::FBOTransmitter2::initThreads() {
    if (!connected_) {
#ifdef _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Connecting ...\n");
#endif
#ifdef MEGAMOL_USE_MPI

        if ((aggregate_ && mpiRank == 0) || !aggregate_) {
#ifdef _DEBUG
            megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Connecting rank %d\n", mpiRank);
#endif
#endif // MEGAMOL_USE_MPI
            auto const address =
                std::string(T2A(this->address_slot_.Param<megamol::core::param::StringParam>()->Value()));
            auto const target =
                std::string(T2A(this->target_machine_slot_.Param<megamol::core::param::StringParam>()->Value()));
            auto const handshake =
                std::to_string(this->handshake_port_slot_.Param<megamol::core::param::IntParam>()->Value());


            FBOCommFabric registerComm = FBOCommFabric{std::make_unique<ZMQCommFabric>(zmq::socket_type::req)};
            std::string const registerAddress = std::string("tcp://") + target + std::string(":") + handshake;

#if _DEBUG
            megamol::core::utility::log::Log::DefaultLog.WriteInfo(
                "FBOTransmitter2: registerAddress: %s\n", registerAddress.c_str());
#endif
            registerComm.Connect(registerAddress);

            std::string hostname = std::string{"127.0.0.1"};
            if (!this->force_localhost_slot_.Param<megamol::core::param::BoolParam>()->Value()) {
                hostname.clear();
#if _WIN32
                DWORD buf_size = MAX_COMPUTERNAME_LENGTH + 1;
                hostname.resize(buf_size);
                GetComputerNameA(hostname.data(), &buf_size);
#else
            hostname.resize(HOST_NAME_MAX);
            gethostname(hostname.data(), HOST_NAME_MAX);
#endif
            }
            char stuff[1024];
            sprintf(stuff, "tcp://%s:%s", hostname.c_str(), address.c_str());
            auto name = std::string{stuff};
            std::vector<char> buf(name.begin(), name.end()); //<TODO there should be a better way
            try {
#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo(
                    "FBOTransmitter2: Sending client name %s\n", name.c_str());
#endif
                if (!registerComm.Send(buf)) {
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "FBOTransmitter2: Send on 'registerComm' failed\n");
                }
#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Sent client name\n");
#endif
#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Receiving client ack\n");
#endif
                while (!registerComm.Recv(buf)) {
#if _DEBUG
                    megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                        "FBOTransmitter2: Recv failed on 'registerComm', trying again\n");
#endif
                }
#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Received client ack\n");
#endif


#if _DEBUG
                megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Connecting comm\n");
#endif
            } catch (std::exception& e) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "FBOTransmitter2: Register died: %s\n", e.what());
            } catch (vislib::Exception& e) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "FBOTransmitter2: Register died: %s\n", e.GetMsgA());
            } catch (...) {
                megamol::core::utility::log::Log::DefaultLog.WriteError("FBOTransmitter2: Register died\n");
            }

            auto const comm_type = static_cast<FBOCommFabric::commtype>(
                this->commSelectSlot_.Param<megamol::core::param::EnumParam>()->Value());
            // auto const address =
            // std::string(T2A(this->address_slot_.Param<megamol::core::param::StringParam>()->Value()));
            switch (comm_type) {
            case FBOCommFabric::MPI_COMM: {
                int const rank = atoi(address.c_str());
                this->comm_.reset(new FBOCommFabric{std::make_unique<MPICommFabric>(rank, rank)});
            } break;
            case FBOCommFabric::ZMQ_COMM:
            default:
                this->comm_.reset(new FBOCommFabric(std::make_unique<ZMQCommFabric>(zmq::socket_type::rep)));
            }

            this->comm_->Bind(std::string{"tcp://*:"} + address);

            this->thread_stop_ = false;

            this->transmitter_thread_ = std::thread(&FBOTransmitter2::transmitterJob, this);

            megamol::core::utility::log::Log::DefaultLog.WriteInfo("FBOTransmitter2: Connection established.\n");

#ifdef MEGAMOL_USE_MPI
        }
#endif // MEGAMOL_USE_MPI
        connected_ = true;
    }

    return true;
}


bool megamol::remote::FBOTransmitter2::shutdownThreads() {
    this->thread_stop_ = true;
    // shutdown_ = true;

    if (this->transmitter_thread_.joinable())
        this->transmitter_thread_.join();

#ifdef MEGAMOL_USE_MPI
    if (useMpi) {
        icetDestroyMPICommunicator(icet_comm_);
        icetDestroyContext(icet_ctx_);
    }
#endif // MEGAMOL_USE_MPI

    connected_ = false;
    return true;
}

bool megamol::remote::FBOTransmitter2::renderCompChanged(core::param::ParamSlot& slot) {
    using megamol::core::utility::log::Log;

    initIceT();

    bool success = true;
    std::string mvn(view_name_slot_.Param<megamol::core::param::StringParam>()->Value());

    Log::DefaultLog.WriteInfo("FBOTransmitter2: Rendering to of \"%s\" requested", mvn.c_str());

    const auto ret = this->GetCoreInstance()->FindModuleNoLock<megamol::core::view::AbstractView>(
        mvn, [this](megamol::core::view::AbstractView* vi) { vi->RegisterHook(this); });
    if (!ret) {
        Log::DefaultLog.WriteError("FBOTransmitter2: Unable to find VIEW \"%s\" for transmission", mvn.c_str());
        success = false;
    }

    return true;
}

void megamol::remote::FBOTransmitter2::initIceT() {
#ifdef MEGAMOL_USE_MPI

    useMpi = initMPI();
    aggregate_ = this->toggle_aggregate_slot_.Param<megamol::core::param::BoolParam>()->Value();
    if (aggregate_ && !useMpi) {
        megamol::core::utility::log::Log::DefaultLog.WriteError("Cannot aggregate without MPI!\n");
        this->toggle_aggregate_slot_.Param<megamol::core::param::BoolParam>()->SetValue(false);
    }

    if (aggregate_) {
#if _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo(
            "FBOTransmitter2: Initializing IceT at rank %d\n", mpiRank);
#endif
        // icet setup

        icet_comm_ = icetCreateMPICommunicator(this->mpi_comm_);
        icet_ctx_ = icetCreateContext(icet_comm_);
        icetStrategy(ICET_STRATEGY_SEQUENTIAL);
        icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC);
        icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
        icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
        icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
        icetDisable(ICET_COMPOSITE_ONE_BUFFER);

        // extract viewport or get if from opengl context
        auto width = 0;
        auto height = 0;
        if (!this->tiled_slot_.Param<core::param::BoolParam>()->Value()) {
            GLint glvp[4];
            glGetIntegerv(GL_VIEWPORT, glvp);
            for (int i = 0; i < 4; ++i) {
                this->viewport[i] = glvp[i];
            }
            width = this->viewport[4] = glvp[2];
            height = this->viewport[5] = glvp[3];
        } else {
            if (this->extractViewport(this->viewport)) {
                this->validViewport = true;
                width = this->viewport[4];
                height = this->viewport[5];
            } else {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "FBOTransmitter2: ViewPortExtraction - extractViewport failed\n");
                return;
            }
        }

#ifdef _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo(
            "FBOTransmitter2: IceT viewport for rank %d extracted from %s: (%d, %d, %d, %d, %d, %d).", this->mpiRank,
            ((this->validViewport) ? ("View") : ("OpenGL")), this->viewport[0], this->viewport[1], this->viewport[2],
            this->viewport[3], this->viewport[4], this->viewport[5]);
#endif

        int displayRank = 0;
        icetPhysicalRenderSize(width, height);
        icetResetTiles();
        icetAddTile(0, 0, width, height, displayRank);

#ifdef _DEBUG
        megamol::core::utility::log::Log::DefaultLog.WriteInfo(
            "FBOTransmitter2: Initialized IceT at rank %d\n", mpiRank);
#endif
    }
#endif // MEGAMOL_USE_MPI
}

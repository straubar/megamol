/*
 * Remote_Service.cpp
 *
 * Copyright (C) 2021 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "Remote_Service.hpp"

#include "HeadNode.hpp"
#include "MPI_Context.h"
#include "MpiNode.hpp"
#include "RenderNode.hpp"

#include "mmcore/MegaMolGraph.h"

#include "GUIRegisterWindow.h" // register UI window for remote control
#include "imgui_stdlib.h"

#include "mmcore/utility/log/Log.h"

static const std::string service_name = "Remote_Service: ";
static void log(std::string const& text) {
    const std::string msg = service_name + text;
    megamol::core::utility::log::Log::DefaultLog.WriteInfo(msg.c_str());
}

static void log_error(std::string const& text) {
    const std::string msg = service_name + text;
    megamol::core::utility::log::Log::DefaultLog.WriteError(msg.c_str());
}

static void log_warning(std::string const& text) {
    const std::string msg = service_name + text;
    megamol::core::utility::log::Log::DefaultLog.WriteWarn(msg.c_str());
}


namespace megamol::frontend {

using frontend_resources::MPI_Context;

std::string handle_remote_session_config(
    megamol::frontend_resources::RuntimeConfig const& config, Remote_Service::Config& remote_config) {
    std::string role_name;

    if (config.remote_headnode) {
        remote_config.role = megamol::frontend::Remote_Service::Role::HeadNode;
        role_name = " (Head Node)";
    }
    if (config.remote_rendernode) {
        remote_config.role = megamol::frontend::Remote_Service::Role::RenderNode;
        role_name = " (Render Node)";
    }
    if (config.remote_mpirendernode) {
#ifdef MEGAMOL_USE_MPI
        remote_config.role = megamol::frontend::Remote_Service::Role::MPIRenderNode;
        role_name = " (MPI Render Node)";
#else
        log_error(" MegaMol was compiled without MPI. Can not start in MPI mode. Shut down.");
        std::exit(1);
#endif // MEGAMOL_USE_MPI
    }

    remote_config.mpi_broadcast_rank = config.remote_mpi_broadcast_rank;
    remote_config.headnode_zmq_target_address = config.remote_headnode_zmq_target_address;
    remote_config.rendernode_zmq_source_address = config.remote_rendernode_zmq_source_address;
    remote_config.headnode_broadcast_quit = config.remote_headnode_broadcast_quit;
    remote_config.headnode_broadcast_initial_project = config.remote_headnode_broadcast_initial_project;
    remote_config.headnode_connect_on_start = config.remote_headnode_connect_on_start;

    //remote_config.render_sync_data_sources_mpi;
    //remote_config.render_use_mpi;

    return role_name;
}


struct Remote_Service::PimplData {
    HeadNode head;
    bool is_headnode_running = false;
    RenderNode render;
    MpiNode mpi;
    MPI_Context mpi_context;
    megamol::remote::Message_t message;
};
#define m_head (m_pimpl->head)
#define m_is_headnode_running (m_pimpl->is_headnode_running)
#define m_render (m_pimpl->render)
#define m_mpi (m_pimpl->mpi)
#define m_mpi_context (m_pimpl->mpi_context)
#define m_message (m_pimpl->message)

Remote_Service::Remote_Service() {
    // init members to default states
}

Remote_Service::~Remote_Service() {
    // clean up raw pointers you allocated with new, which is bad practice and nobody does
}

bool Remote_Service::init(void* configPtr) {
    if (configPtr == nullptr)
        return false;

    return init(*static_cast<Config*>(configPtr));
}

bool Remote_Service::init(const Config& config) {
    m_pimpl =
        std::unique_ptr<PimplData, std::function<void(PimplData*)>>(new PimplData, [](PimplData* ptr) { delete ptr; });
    m_config = config;

    if (false) {
        log_error("failed initialization because");
        return false;
    }

    this->m_providedResourceReferences = {};

    this->m_requestedResourcesNames = {"MegaMolGraph",
        "ExecuteLuaScript" // std::function<std::tuple<bool,std::string>(std::string const&)>
        ,
        "optional<GUIRegisterWindow>"};

    m_do_remote_things = std::function{[&]() {}};

    if (config.role == Role::HeadNode) {
        if (config.headnode_connect_on_start && !start_headnode(true))
            return false;

        if (config.headnode_broadcast_initial_project) {
            m_headnode_remote_control.commands_queue.push_back(HeadNodeRemoteControl::Command::SendGraph);
        }
    }

    if (config.role == Role::RenderNode) {
        m_do_remote_things = std::function{[&]() { do_rendernode_things(); }};

        if (!m_render.start_receiver(config.rendernode_zmq_source_address)) {
            log_error("could not start RenderNode receiver");
            return false;
        }
    }

    if (config.role == Role::MPIRenderNode) {
        m_do_remote_things = std::function{[&]() { do_mpi_things(); }};

        if (!m_mpi.init(static_cast<int>(config.mpi_broadcast_rank))) {
            log_error("could not start MPI");
            return false;
        }

        if (m_mpi.mpi_comm.do_i_broadcast()) {
            if (!m_render.start_receiver(config.rendernode_zmq_source_address)) {
                log_error("could not start RenderNode receiver");
                return false;
            }
        }

        // copy MPI context/comm info to global resource
        // here we hope the MPI_Comm struct defined in mpi.h is ok with beeing copied
        m_mpi_context = m_mpi.mpi_comm;

        this->m_providedResourceReferences.push_back({"MPI_Context", m_mpi_context});
    }

    log("initialized successfully");
    return true;
}

void Remote_Service::close() {
    switch (m_config.role) {
    case Role::HeadNode:
        start_headnode(false);
        break;
    case Role::RenderNode:
        m_render.close_receiver();
        break;
    case Role::MPIRenderNode:
        if (m_mpi.i_do_broadcast())
            m_render.close_receiver();
        m_mpi.close();
        break;
    default:
        break;
    }
}

std::vector<FrontendResource>& Remote_Service::getProvidedResources() {
    return m_providedResourceReferences;
}

const std::vector<std::string> Remote_Service::getRequestedResourceNames() const {
    return m_requestedResourcesNames;
}

void Remote_Service::setRequestedResources(std::vector<FrontendResource> resources) {
    this->m_requestedResourceReferences = resources;

    if (m_config.role == Role::HeadNode) {
        remote_control_window();
    }
}

void Remote_Service::updateProvidedResources() {}

void Remote_Service::digestChangedRequestedResources() {
    // depending on role does head or render things
    m_do_remote_things();

    if (false)
        this->setShutdown();
}

void Remote_Service::resetProvidedResources() {}

void Remote_Service::preGraphRender() {}

void Remote_Service::postGraphRender() {}

void Remote_Service::do_headnode_things() {
    auto& graph = m_requestedResourceReferences[0].getResource<megamol::core::MegaMolGraph>();

    for (auto command : m_headnode_remote_control.commands_queue)
        switch (command) {
        case HeadNodeRemoteControl::Command::ClearGraph:
            head_send_message("mmClearGraph()");
            break;
        case HeadNodeRemoteControl::Command::SendGraph:
            head_send_message(const_cast<megamol::core::MegaMolGraph&>(graph).Convenience().SerializeGraph());
            break;
        case HeadNodeRemoteControl::Command::SendLuaCommand:
            head_send_message(m_headnode_remote_control.lua_command);
            break;
        default:
            break;
        }
    m_headnode_remote_control.commands_queue.clear();

    auto split_module_names = [&](std::string const& modules_list_string, auto& module_list) {
        if (modules_list_string.empty())
            return;

        const auto delimiters = ", ";
        size_t begin = modules_list_string.find_first_not_of(delimiters);
        auto end = modules_list_string.find_first_of(delimiters, begin);

        while (begin != std::string::npos) {
            module_list.push_back(modules_list_string.substr(begin, end - begin));
            begin = modules_list_string.find_first_not_of(delimiters, end);
            end = modules_list_string.find_first_of(delimiters, begin);
        }
    };

    if (m_headnode_remote_control.keep_sending_params) {
        static std::string send_params;
        send_params.clear();

        if (m_headnode_remote_control.modules_to_send_params_of == "all") {
            send_params = const_cast<megamol::core::MegaMolGraph&>(graph).Convenience().SerializeAllParameters();
        } else {
            static std::vector<std::string> module_list;
            module_list.clear();

            split_module_names(m_headnode_remote_control.modules_to_send_params_of, module_list);

            for (auto& module : module_list)
                send_params +=
                    const_cast<megamol::core::MegaMolGraph&>(graph).Convenience().SerializeModuleParameters(module);
        }

        head_send_message(send_params);
    }
}

void Remote_Service::add_headnode_remote_command(HeadNodeRemoteControl::Command command, std::string const& value) {

    auto uint_command = static_cast<unsigned int>(command);
    if (uint_command > static_cast<unsigned int>(HeadNodeRemoteControl::Command::Count)) {
        log_error("unknown Remote Control command number: " + std::to_string(uint_command) + ", value: " + value);
        return;
    }

    HeadNodeRemoteControl::Command cmd = static_cast<HeadNodeRemoteControl::Command>(command);

    switch (cmd) {
    case HeadNodeRemoteControl::Command::StartHeadNode:
        start_headnode(true);
        break;
    case HeadNodeRemoteControl::Command::CloseHeadNode:
        start_headnode(false);
        break;
    case HeadNodeRemoteControl::Command::KeepSendingParams:
        m_headnode_remote_control.keep_sending_params = true;
        break;
    case HeadNodeRemoteControl::Command::DontSendParams:
        m_headnode_remote_control.keep_sending_params = false;
        break;
    case HeadNodeRemoteControl::Command::SetParamSendingModules:
        m_headnode_remote_control.modules_to_send_params_of = value;
        break;
    case HeadNodeRemoteControl::Command::SendLuaCommand:
        m_headnode_remote_control.lua_command = value;
        break;
    default:
        break;
    }

    m_headnode_remote_control.commands_queue.push_back(cmd);
}

void Remote_Service::remote_control_window() {
    auto maybe_gui_window_request_resource =
        m_requestedResourceReferences[2].getOptionalResource<megamol::frontend_resources::GUIRegisterWindow>();

    if (!maybe_gui_window_request_resource.has_value()) {
        return;
    }

    auto& gui_window_request_resource = maybe_gui_window_request_resource.value().get();

    gui_window_request_resource.register_window(
        "Head Node Remote Control", [&](megamol::gui::AbstractWindow::BasicConfig& window_config) {
            window_config.flags = ImGuiWindowFlags_AlwaysAutoResize;

            static bool keep_sending_params = false;
            static std::string param_send_modules = "all";
            static std::string lua_command = "mmQuit()";
            if (m_is_headnode_running) {
                ImGui::Text("(Server RUNNING) ");
                ImGui::SameLine();
                if (ImGui::Button("Stop Head Node Server")) {
                    add_headnode_remote_command(HeadNodeRemoteControl::Command::CloseHeadNode);
                }
            } else {
                ImGui::Text("(Server STOPPED) ");
                ImGui::SameLine();
                if (ImGui::Button("Start Head Node Server")) {
                    add_headnode_remote_command(HeadNodeRemoteControl::Command::StartHeadNode);
                }
            }
            ImGui::SameLine();
            ImGui::InputText("ZMQ Target", &m_config.headnode_zmq_target_address);

            if (ImGui::Button("Broadcast Local Graph")) {
                add_headnode_remote_command(HeadNodeRemoteControl::Command::SendGraph);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Rendernode Graphs")) {
                add_headnode_remote_command(HeadNodeRemoteControl::Command::DontSendParams);
                add_headnode_remote_command(HeadNodeRemoteControl::Command::ClearGraph);
                keep_sending_params = false;
            }

            if (ImGui::RadioButton("Sync Module Params", keep_sending_params)) {
                keep_sending_params = !keep_sending_params;
                keep_sending_params ? add_headnode_remote_command(HeadNodeRemoteControl::Command::KeepSendingParams)
                                    : add_headnode_remote_command(HeadNodeRemoteControl::Command::DontSendParams);
            }
            ImGui::SameLine();
            if (ImGui::InputText("Sync Modules", &param_send_modules, ImGuiInputTextFlags_EnterReturnsTrue)) {
                add_headnode_remote_command(HeadNodeRemoteControl::Command::SetParamSendingModules, param_send_modules);
            }

            if (ImGui::Button("Send Lua Command")) {
                add_headnode_remote_command(HeadNodeRemoteControl::Command::SendLuaCommand, lua_command);
            }
            ImGui::SameLine();
            ImGui::InputText("Lua Command", &lua_command);
        });
}

bool Remote_Service::start_headnode(bool start_or_shutdown) {
    switch (start_or_shutdown) {
    case true:
        m_do_remote_things = std::function{[&]() { do_headnode_things(); }};

        if (!m_head.start_server(m_config.headnode_zmq_target_address)) {
            log_error("could not start HeadNode server");
            return false;
        }
        m_is_headnode_running = true;
        break;
    case false:
        if (m_config.headnode_broadcast_quit)
            head_send_message("mmQuit()");
        m_head.close_server();
        m_is_headnode_running = false;
        break;
    };
    return true;
}

void Remote_Service::do_rendernode_things() {
    m_message.clear();

    if (m_render.await_message(m_message, 3)) {
        execute_message(m_message);
    }
}

void Remote_Service::do_mpi_things() {
    m_message.clear();

    if (m_mpi.i_do_broadcast()) {
        if (m_render.await_message(m_message, 3)) {}
    }

    m_mpi.get_broadcast_message(m_message);

    execute_message(m_message);

    m_mpi.sync_barrier();
}

void Remote_Service::head_send_message(std::string const& string) {
    m_message.resize(string.size());
    std::memcpy(m_message.data(), string.data(), string.size());
    m_head.send(m_message);
}

void Remote_Service::execute_message(std::vector<char> const& message) {
    if (message.empty())
        return;

    static std::string commands_string;
    commands_string.resize(message.size());
    std::memcpy(commands_string.data(), message.data(), message.size());

    auto& executeLua = m_requestedResourceReferences[1]
                           .getResource<std::function<std::tuple<bool, std::string>(std::string const&)>>();
    auto result = executeLua(commands_string);

    if (!std::get<0>(result)) {
        log_error("Error executing Lua: " + std::get<1>(result));
    }
}


// ===================================================================================================
// === BELOW: CODE THAT MAY NEED TO BE PORTED SOME DAY ===============================================
// ===================================================================================================


#if (0)

// identifier for sent messages. counts up for each message.
uint64_t msg_id_ = 0;

// need to wrap sent messages according to convention

// HEAD NODE SENDING

for (auto const& el : updates) {
    std::vector<char> msg;

    std::string mg = "mmSetParamValue(\"" + el.first + "\", \"" + el.second + "\")";

    msg.resize(MessageHeaderSize + mg.size());
    msg[0] = static_cast<char>(MessageType::PARAM_UPD_MSG);
    auto size = mg.size();
    std::copy(reinterpret_cast<char*>(&size), reinterpret_cast<char*>(&size) + MessageSizeSize,
        msg.begin() + MessageTypeSize);
    ++msg_id_;
    std::copy(reinterpret_cast<char*>(&msg_id_), reinterpret_cast<char*>(&msg_id_) + MessageIDSize,
        msg.begin() + MessageTypeSize + MessageSizeSize);
    std::copy(mg.begin(), mg.end(), msg.begin() + MessageHeaderSize);
}


bool megamol::remote::HeadnodeServer::get_cam_upd(std::vector<char>& msg) {

    AbstractNamedObject::const_ptr_type avp;
    const core::view::AbstractView* av = nullptr;
    core::view::CallRenderViewGL* call = nullptr;
    unsigned int csn = 0;

    av = nullptr;
    call = this->view_slot_.CallAs<core::view::CallRenderViewGL>();
    if ((call != nullptr) && (call->PeekCalleeSlot() != nullptr) && (call->PeekCalleeSlot()->Parent() != nullptr)) {
        avp = call->PeekCalleeSlot()->Parent();
        av = dynamic_cast<const core::view::AbstractView*>(avp.get());
    }
    if (av == nullptr)
        return false;

    csn = av->GetCameraSyncNumber();
    if ((csn != syncnumber)) {
        syncnumber = csn;
        core::view::Camera_2 cam;
        call->GetCamera(cam);
        cam_type::snapshot_type snapshot;
        cam_type::matrix_type viewTemp, projTemp;
        cam.calc_matrices(snapshot, viewTemp, projTemp, core::thecam::snapshot_content::all);
        cam_type::minimal_state_type min_state;
        cam.get_minimal_state(min_state);

        core::view::CameraSerializer serializer;
        const std::string mem = serializer.serialize(min_state);


        msg.resize(MessageHeaderSize + mem.size());
        msg[0] = static_cast<char>(MessageType::CAM_UPD_MSG);
        auto size = mem.size();
        std::copy(reinterpret_cast<char*>(&size), reinterpret_cast<char*>(&size) + MessageSizeSize,
            msg.begin() + MessageTypeSize);
        std::copy(mem.data(), mem.data() + mem.size(), msg.begin() + MessageHeaderSize);

        return true;
    }

    return false;
}

bool megamol::remote::HeadnodeServer::onLuaCommand(core::param::ParamSlot& param) {
    if (!run_threads_)
        return true;

    std::vector<char> msg;
    std::string mg = std::string(param.Param<core::param::StringParam>()->ValueString());

    msg.resize(MessageHeaderSize + mg.size());
    msg[0] = static_cast<char>(MessageType::PARAM_UPD_MSG);
    auto size = mg.size();
    std::copy(reinterpret_cast<char*>(&size), reinterpret_cast<char*>(&size) + MessageSizeSize,
        msg.begin() + MessageTypeSize);
    ++msg_id_;
    std::copy(reinterpret_cast<char*>(&msg_id_), reinterpret_cast<char*>(&msg_id_) + MessageIDSize,
        msg.begin() + MessageTypeSize + MessageSizeSize);
    std::copy(mg.begin(), mg.end(), msg.begin() + MessageHeaderSize);


    std::lock_guard<std::mutex> guard(send_buffer_guard_);
    send_buffer_.insert(send_buffer_.end(), msg.begin(), msg.end());
    buffer_has_changed_.store(true);

    return true;
}

// retrieve modulgraph
if (this->deploy_project_slot_.Param<core::param::BoolParam>()->Value()) {
    if (this->GetCoreInstance()->IsLuaProject()) {
        auto const lua = std::string(this->GetCoreInstance()->GetMergedLuaProject());
        std::vector<char> msg(MessageHeaderSize + lua.size());
        msg[0] = MessageType::PRJ_FILE_MSG;
        auto size = lua.size();
        std::copy(reinterpret_cast<char*>(&size), reinterpret_cast<char*>(&size) + MessageSizeSize,
            msg.begin() + MessageTypeSize);
        ++msg_id_;
        std::copy(reinterpret_cast<char*>(&msg_id_), reinterpret_cast<char*>(&msg_id_) + MessageIDSize,
            msg.begin() + MessageTypeSize + MessageSizeSize);
        std::copy(lua.begin(), lua.end(), msg.begin() + MessageHeaderSize);
        {
            std::lock_guard<std::mutex> lock(send_buffer_guard_);
            send_buffer_.insert(send_buffer_.end(), msg.begin(), msg.end());
        }
    }
}


bool megamol::remote::RendernodeView::process_msgs(Message_t const& msgs) {
    static uint64_t old_msg_id = -1;

    auto ibegin = msgs.cbegin();
    auto const iend = msgs.cend();

    while (ibegin < iend) {
        auto const type = static_cast<MessageType>(*ibegin);
        auto size = 0;
#ifdef RV_DEBUG_OUTPUT
        uint64_t msg_id = 0;
#endif
        switch (type) {
        case MessageType::PRJ_FILE_MSG: {
            auto const call = this->getCallRenderView();
            if (call != nullptr) {
                this->disconnectOutgoingRenderCall();
                this->GetCoreInstance()->CleanupModuleGraph();
            }
        }
        case MessageType::PARAM_UPD_MSG: {

            if (std::distance(ibegin, iend) > MessageHeaderSize) {
                std::copy(ibegin + MessageTypeSize, ibegin + MessageTypeSize + MessageSizeSize,
                    reinterpret_cast<char*>(&size));
#ifdef RV_DEBUG_OUTPUT
                std::copy(ibegin + MessageTypeSize + MessageSizeSize, ibegin + MessageHeaderSize,
                    reinterpret_cast<char*>(&msg_id));
                if (msg_id - old_msg_id == 1) {
                    megamol::core::utility::log::Log::DefaultLog.WriteInfo(
                        "RendernodeView: Got message with id: %d", msg_id);
                } else {
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "RendernodeView: Unexpected id: %d", msg_id);
                }
                old_msg_id = msg_id;
#endif
            }
            Message_t msg;
            if (std::distance(ibegin, iend) >= MessageHeaderSize + size) {
                msg.resize(size);
                std::copy(ibegin + MessageHeaderSize, ibegin + MessageHeaderSize + size, msg.begin());
            }

            std::string mg(msg.begin(), msg.end());
            std::string result;
            auto const success = this->GetCoreInstance()->GetLuaState()->RunString(mg, result);
            if (!success) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "RendernodeView: Could not queue project file: %s", result.c_str());
            }
        } break;
        case MessageType::CAM_UPD_MSG: {

            /*if (std::distance(ibegin, iend) > MessageHeaderSize) {
                std::copy(ibegin + MessageTypeSize, ibegin + MessageHeaderSize, reinterpret_cast<char*>(&size));
            }
            Message_t msg;
            if (std::distance(ibegin, iend) >= MessageHeaderSize + size) {
                msg.resize(size);
                std::copy(ibegin + MessageHeaderSize, ibegin + MessageHeaderSize + size, msg.begin());
            }
            vislib::RawStorageSerialiser ser(reinterpret_cast<unsigned char*>(msg.data()), msg.size());
            auto view = this->getConnectedView();
            if (view != nullptr) {
                view->DeserialiseCamera(ser);
            } else {
                megamol::core::utility::log::Log::DefaultLog.WriteError("RendernodeView: Cannot update camera. No view connected.");
            }*/
        } break;
        case MessageType::HEAD_DISC_MSG:
        case MessageType::NULL_MSG:
            break;
        default:
            megamol::core::utility::log::Log::DefaultLog.WriteWarn("RendernodeView: Unknown msg type.");
        }
        ibegin += size + MessageHeaderSize;
    }

    return true;
}


#endif
// TODO
//        int fnameDirty = ss->getFilenameDirty();
//        int allFnameDirty = 0;
//        MPI_Allreduce(&fnameDirty, &allFnameDirty, 1, MPI_INT, MPI_LAND, this->comm_);
//#    ifdef RV_DEBUG_OUTPUT
//        megamol::core::utility::log::Log::DefaultLog.WriteInfo("Remote_Service::MpiNode: allFnameDirty: %d", allFnameDirty);
//#    endif
//
//        if (allFnameDirty) {
//            if (!(*ss)(1)) { // finally set the filename in the data source
//                megamol::core::utility::log::Log::DefaultLog.WriteError("Remote_Service::MpiNode: SyncData SetFilename callback failed.");
//                return;
//            }
//            ss->resetFilenameDirty();
//        }

// #if(0)
// // #include "Remote_Service::MpiNode.h"
//
// #include <array>
//
// #include "mmcore/CoreInstance.h"
// #include "mmcore/param/BoolParam.h"
// #include "mmcore/param/IntParam.h"
// #include "mmcore/param/StringParam.h"
//
// #include "mmcore/cluster/SyncDataSourcesCall.h"
// #include "mmcore/cluster/mpi/MpiCall.h"
// #include "vislib/RawStorageSerialiser.h"
//
// //  _fbo = std::make_shared<vislib_gl::graphics::gl::FramebufferObject>();
//
//
// void megamol::remote::Remote_Service::MpiNode::Render(const mmcRenderViewContext& context, core::Call* call) {
// ifdef MEGAMOL_USE_MPI
//     static bool first_frame = true;
//
//     this->initMPI();
//
//     if (first_frame) {
//         this->initTileViewParameters();
//         this->checkParameters();
//         first_frame = false;
//     }
//
//     // 0 time, 1 instanceTime
//     std::array<double, 2> timestamps = {0.0, 0.0};
//
//     // if broadcastmaster, start listening thread
//     // auto isBCastMaster = isBCastMasterSlot_.Param<core::param::BoolParam>->Value();
//     // auto BCastRank = BCastRankSlot_.Param<core::param::IntParam>->Value();
//     auto const BCastMaster = isBCastMaster();
//     if (!threads_initialized_ && BCastMaster) {
//         init_threads();
//     }
//
//     // BROADCAST HAPPENED HERE
//
//     // initialize rendering
//     auto ss = this->sync_data_slot_.CallAs<core::cluster::SyncDataSourcesCall>();
//     if (ss != nullptr) {
//         if (!(*ss)(0)) { // check for dirty filenamesslot
//             megamol::core::utility::log::Log::DefaultLog.WriteError("Remote_Service::MpiNode: SyncData GetDirty callback failed.");
//             return;
//         }
//         int fnameDirty = ss->getFilenameDirty();
//         int allFnameDirty = 0;
//         MPI_Allreduce(&fnameDirty, &allFnameDirty, 1, MPI_INT, MPI_LAND, this->comm_);
// #    ifdef RV_DEBUG_OUTPUT
//         megamol::core::utility::log::Log::DefaultLog.WriteInfo("Remote_Service::MpiNode: allFnameDirty: %d", allFnameDirty);
// #    endif
//
//         if (allFnameDirty) {
//             if (!(*ss)(1)) { // finally set the filename in the data source
//                 megamol::core::utility::log::Log::DefaultLog.WriteError("Remote_Service::MpiNode: SyncData SetFilename callback failed.");
//                 return;
//             }
//             ss->resetFilenameDirty();
//         }
// #    ifdef RV_DEBUG_OUTPUT
//         if (!allFnameDirty && fnameDirty) {
//             megamol::core::utility::log::Log::DefaultLog.WriteInfo("Remote_Service::MpiNode: Waiting for data in MPI world to be ready.");
//         }
// #    endif
//     } else {
// #    ifdef RV_DEBUG_OUTPUT
//         megamol::core::utility::log::Log::DefaultLog.WriteInfo("Remote_Service::MpiNode: No sync object connected.");
// #    endif
//     }
//     // check whether rendering is possible in current state
//     auto crv = this->getCallRenderView();
//     if (crv != nullptr) {
//         crv->ResetAll();
//         crv->SetTime(static_cast<float>(timestamps[0]));
//         crv->SetInstanceTime(timestamps[1]);
//         crv->SetProjection(this->getProjType(), this->getEye());
//
//         if (this->hasTile()) {
//             crv->SetTile(this->getVirtWidth(), this->getVirtHeight(), this->getTileX(), this->getTileY(),
//                 this->getTileW(), this->getTileH());
//         }
//
//         if ((this->_fbo->GetWidth() != this->getTileW()) || (this->_fbo->GetHeight() != this->getTileH()) ||
//             (!this->_fbo->IsValid())) {
//             this->_fbo->Release();
//             if (!this->_fbo->Create(this->getTileW(), this->getTileH(), GL_RGBA8, GL_RGBA,
//                     GL_UNSIGNED_BYTE, vislib_gl::graphics::gl::FramebufferObject::ATTACHMENT_TEXTURE,
//                     GL_DEPTH_COMPONENT)) {
//                 throw vislib::Exception("[View3DGL] Unable to create image framebuffer object.", __FILE__, __LINE__);
//                 return;
//             }
//         }
//         this->_fbo->Enable();
//         auto bgcol = this->BackgroundColor();
//         glClearColor(bgcol.r, bgcol.g, bgcol.b, bgcol.a);
//         glClearDepth(1.0f);
//         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//         crv->SetFramebufferObject(this->_fbo);
//
//
//         if (!crv->operator()(core::view::CallRenderViewGL::CALL_RENDER)) {
//             megamol::core::utility::log::Log::DefaultLog.WriteError("Remote_Service::MpiNode: Failed to call render on dependend view.");
//         }
//
//         this->_fbo->Disable();
//         this->_fbo->DrawColourTexture();
//
//
//         glFinish();
//     } else {
// #    ifdef RV_DEBUG_OUTPUT
//         megamol::core::utility::log::Log::DefaultLog.WriteWarn("Remote_Service::MpiNode: crv_ is nullptr.\n");
// #    endif
//     }
//
//     // sync barrier
//     MPI_Barrier(this->comm_);
// #endif
// }
//
// #endif
//

} // namespace megamol::frontend

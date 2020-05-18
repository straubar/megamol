/*
 * Parameter.cpp
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "Parameter.h"


using namespace megamol;
using namespace megamol::gui;
using namespace megamol::gui::configurator;


megamol::gui::configurator::Parameter::Parameter(
    ImGuiID uid, ParamType type, StroageType store, MinType min, MaxType max)
    : uid(uid)
    , type(type)
    , full_name()
    , description()
    , minval(min)
    , maxval(max)
    , storage(store)
    , value()
    , default_value()
    , default_value_mismatch(false)
    , present() {

    // Initialize variant types which should/can not be changed afterwards.
    // Default ctor of variants initializes std::monostate.
    switch (this->type) {
    case (Parameter::ParamType::BOOL): {
        this->value = bool(false);
    } break;
    case (Parameter::ParamType::BUTTON): {
        // set_default_value_mismatch = true;
    } break;
    case (Parameter::ParamType::COLOR): {
        this->value = megamol::core::param::ColorParam::ColorType();
    } break;
    case (Parameter::ParamType::ENUM): {
        this->value = int(0);
    } break;
    case (Parameter::ParamType::FILEPATH): {
        this->value = std::string();
    } break;
    case (Parameter::ParamType::FLEXENUM): {
        this->value = std::string();
    } break;
    case (Parameter::ParamType::FLOAT): {
        this->value = float(0.0f);
    } break;
    case (Parameter::ParamType::INT): {
        this->value = int();
    } break;
    case (Parameter::ParamType::STRING): {
        this->value = std::string();
    } break;
    case (Parameter::ParamType::TERNARY): {
        this->value = vislib::math::Ternary();
    } break;
    case (Parameter::ParamType::TRANSFERFUNCTION): {
        this->value = std::string();
    } break;
    case (Parameter::ParamType::VECTOR2F): {
        this->value = glm::vec2();
    } break;
    case (Parameter::ParamType::VECTOR3F): {
        this->value = glm::vec3();
    } break;
    case (Parameter::ParamType::VECTOR4F): {
        this->value = glm::vec4();
    } break;
    default:
        break;
    }

    this->default_value = this->value;
}


megamol::gui::configurator::Parameter::~Parameter(void) {}


std::string megamol::gui::configurator::Parameter::GetValueString(void) {
    std::string value_string = "UNKNOWN PARAMETER TYPE";
    auto visitor = [this, &value_string](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            auto parameter = megamol::core::param::BoolParam(arg);
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, megamol::core::param::ColorParam::ColorType>) {
            auto parameter = megamol::core::param::ColorParam(arg);
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, float>) {
            auto parameter = megamol::core::param::FloatParam(arg);
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, int>) {
            switch (this->type) {
            case (Parameter::ParamType::INT): {
                auto parameter = megamol::core::param::IntParam(arg);
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            case (Parameter::ParamType::ENUM): {
                auto parameter = megamol::core::param::EnumParam(arg);
                // Initialization of enum storage required
                auto map = this->GetStorage<EnumStorageType>();
                for (auto& pair : map) {
                    parameter.SetTypePair(pair.first, pair.second.c_str());
                }
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            default:
                break;
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            switch (this->type) {
            case (Parameter::ParamType::STRING): {
                auto parameter = megamol::core::param::StringParam(arg.c_str());
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            case (Parameter::ParamType::TRANSFERFUNCTION): {
                auto parameter = megamol::core::param::TransferFunctionParam(arg);
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            case (Parameter::ParamType::FILEPATH): {
                auto parameter = megamol::core::param::FilePathParam(arg.c_str());
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            case (Parameter::ParamType::FLEXENUM): {
                auto parameter = megamol::core::param::FlexEnumParam(arg.c_str());
                value_string = std::string(parameter.ValueString().PeekBuffer());
            } break;
            default:
                break;
            }
        } else if constexpr (std::is_same_v<T, vislib::math::Ternary>) {
            auto parameter = megamol::core::param::TernaryParam(arg);
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            auto parameter = megamol::core::param::Vector2fParam(vislib::math::Vector<float, 2>(arg.x, arg.y));
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            auto parameter = megamol::core::param::Vector3fParam(vislib::math::Vector<float, 3>(arg.x, arg.y, arg.z));
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            auto parameter =
                megamol::core::param::Vector4fParam(vislib::math::Vector<float, 4>(arg.x, arg.y, arg.z, arg.w));
            value_string = std::string(parameter.ValueString().PeekBuffer());
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            switch (this->type) {
            case (Parameter::ParamType::BUTTON): {
                auto parameter = megamol::core::param::ButtonParam();
                value_string = std::string(parameter.ValueString().PeekBuffer());
                break;
            }
            default:
                break;
            }
        }
    };
    std::visit(visitor, this->value);
    return value_string;
}


bool megamol::gui::configurator::Parameter::SetValueString(const std::string& val_str, bool set_default_val) {

    bool retval = false;
    vislib::TString val_tstr(val_str.c_str());

    switch (this->type) {
    case (Parameter::ParamType::BOOL): {
        megamol::core::param::BoolParam parameter(false);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::BUTTON): {
        retval = true;
    } break;
    case (Parameter::ParamType::COLOR): {
        megamol::core::param::ColorParam parameter(val_tstr);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::ENUM): {
        megamol::core::param::EnumParam parameter(0);
        // Initialization of enum storage required
        auto map = this->GetStorage<EnumStorageType>();
        for (auto& pair : map) {
            parameter.SetTypePair(pair.first, pair.second.c_str());
        }
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::FILEPATH): {
        megamol::core::param::FilePathParam parameter(val_tstr.PeekBuffer());
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(std::string(parameter.Value().PeekBuffer()), set_default_val);
    } break;
    case (Parameter::ParamType::FLEXENUM): {
        megamol::core::param::FlexEnumParam parameter(val_str);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::FLOAT): {
        megamol::core::param::FloatParam parameter(0.0f);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::INT): {
        megamol::core::param::IntParam parameter(0);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::STRING): {
        megamol::core::param::StringParam parameter(val_tstr.PeekBuffer());
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(std::string(parameter.Value().PeekBuffer()), set_default_val);
    } break;
    case (Parameter::ParamType::TERNARY): {
        megamol::core::param::TernaryParam parameter(vislib::math::Ternary::TRI_UNKNOWN);
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::TRANSFERFUNCTION): {
        megamol::core::param::TransferFunctionParam parameter;
        retval = parameter.ParseValue(val_tstr);
        this->SetValue(parameter.Value(), set_default_val);
    } break;
    case (Parameter::ParamType::VECTOR2F): {
        megamol::core::param::Vector2fParam parameter(vislib::math::Vector<float, 2>(0.0f, 0.0f));
        retval = parameter.ParseValue(val_tstr);
        auto val = parameter.Value();
        this->SetValue(glm::vec2(val.X(), val.Y()), set_default_val);
    } break;
    case (Parameter::ParamType::VECTOR3F): {
        megamol::core::param::Vector3fParam parameter(vislib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f));
        retval = parameter.ParseValue(val_tstr);
        auto val = parameter.Value();
        this->SetValue(glm::vec3(val.X(), val.Y(), val.Z()), set_default_val);
    } break;
    case (Parameter::ParamType::VECTOR4F): {
        megamol::core::param::Vector4fParam parameter(vislib::math::Vector<float, 4>(0.0f, 0.0f, 0.0f, 0.0f));
        retval = parameter.ParseValue(val_tstr);
        auto val = parameter.Value();
        this->SetValue(glm::vec4(val.X(), val.Y(), val.Z(), val.W()), set_default_val);
    } break;
    default:
        break;
    }

    return retval;
}


// PARAMETER PRESENTATION ####################################################

megamol::gui::configurator::Parameter::Presentation::Presentation(void)
    : presentation(Presentations::DEFAULT)
    , read_only(false)
    , visible(true)
    , expert(false)
    , help()
    , utils()
    , file_utils()
    , show_tf_editor(false)
    , tf_editor()
    , widget_store()
    , float_format("%.7f")
    , height(0.0f)
    , set_focus(0) {}


megamol::gui::configurator::Parameter::Presentation::~Presentation(void) {}


bool megamol::gui::configurator::Parameter::Presentation::Present(
    megamol::gui::configurator::Parameter& inout_parameter) {

    if (ImGui::GetCurrentContext() == nullptr) {
        vislib::sys::Log::DefaultLog.WriteError(
            "No ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    try {
        if (this->visible || this->expert) {
            ImGui::BeginGroup();
            ImGui::PushID(inout_parameter.uid);

            if (this->expert) {
                this->present_prefix();
                ImGui::SameLine();
            }

            this->present_value_DEFAULT(inout_parameter);
            ImGui::SameLine();
            this->present_postfix(inout_parameter);

            ImGui::PopID();
            ImGui::EndGroup();

            // Additional alternative parameter presentations
            switch (this->presentation) {
            case (Presentations::PIN_VALUE_TO_MOUSE): {
                this->present_value_PIN_VALUE_TO_MOUSE(inout_parameter);
            } break;
            default:
                break;
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


float megamol::gui::configurator::Parameter::Presentation::GetHeight(Parameter& inout_parameter) {


    float height = (ImGui::GetFrameHeightWithSpacing() * (1.15f));
    if (inout_parameter.type == Parameter::ParamType::TRANSFERFUNCTION) {
        if (this->show_tf_editor) {
            height = (ImGui::GetFrameHeightWithSpacing() * (10.0f) + (150.0f + 30.0f));
        } else {
            height = (ImGui::GetFrameHeightWithSpacing() * (1.5f));
        }
    }
    return height;
}


bool megamol::gui::configurator::Parameter::Presentation::presentation_button(void) {

    bool retval = false;

    this->utils.PointCircleButton("", (this->presentation != Presentations::DEFAULT));
    if (ImGui::BeginPopupContextItem("param_present_button_context", 0)) { // 0 = left mouse button
        for (size_t i = 0; i < static_cast<size_t>(Presentations::__COUNT__); i++) {
            std::string presentation_str;
            auto presentation_i = static_cast<Presentations>(i);
            switch (presentation_i) {
            case (Presentations::DEFAULT):
                presentation_str = "Default";
                break;
            case (Presentations::PIN_VALUE_TO_MOUSE):
                presentation_str = "Pin Value to Mouse";
                break;
            default:
                break;
            }
            if (!presentation_str.empty()) {
                if (ImGui::MenuItem(presentation_str.c_str(), nullptr, (presentation_i == this->presentation))) {
                    this->presentation = presentation_i;
                    retval = true;
                }
            }
        }
        ImGui::EndPopup();
    }

    return retval;
}


void megamol::gui::configurator::Parameter::Presentation::present_prefix(void) {

    // Visibility
    if (ImGui::RadioButton("###visible", this->visible)) {
        this->visible = !this->visible;
    }
    this->utils.HoverToolTip("Visibility", ImGui::GetItemID(), 0.5f);

    ImGui::SameLine();

    // Read-only option
    ImGui::Checkbox("###readonly", &this->read_only);
    this->utils.HoverToolTip("Read-Only", ImGui::GetItemID(), 0.5f);

    ImGui::SameLine();

    // Presentation
    this->presentation_button();
    this->utils.HoverToolTip("Presentation", ImGui::GetItemID(), 0.5f);
}


void megamol::gui::configurator::Parameter::Presentation::present_value_DEFAULT(
    megamol::gui::configurator::Parameter& inout_parameter) {

    this->help.clear();

    // Set general proportional item width
    float widget_width = ImGui::GetContentRegionAvail().x * 0.5f;
    ImGui::PushItemWidth(widget_width);

    if (this->read_only) {
        GUIUtils::ReadOnlyWigetStyle(true);
    }

    std::string param_label = inout_parameter.GetName();

    auto visitor = [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            if (ImGui::Checkbox(param_label.c_str(), &arg)) {
                inout_parameter.SetValue(arg);
            }
        } else if constexpr (std::is_same_v<T, megamol::core::param::ColorParam::ColorType>) {
            auto color_flags = ImGuiColorEditFlags_AlphaPreview; // | ImGuiColorEditFlags_Float;
            if (ImGui::ColorEdit4(param_label.c_str(), (float*)arg.data(), color_flags)) {
                inout_parameter.SetValue(arg);
            }
            this->help = "[Click] on the colored square to open a color picker.\n"
                         "[CTRL+Click] on individual component to input value.\n"
                         "[Right-Click] on the individual color widget to show options.";
        } else if constexpr (std::is_same_v<T, float>) {
            if (!std::holds_alternative<T>(this->widget_store)) {
                this->widget_store = arg;
            }
            ImGui::InputFloat(param_label.c_str(), &std::get<float>(this->widget_store), 1.0f, 10.0f,
                this->float_format.c_str(), ImGuiInputTextFlags_None);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                this->widget_store = std::max(inout_parameter.GetMinValue<float>(),
                    std::min(std::get<float>(this->widget_store), inout_parameter.GetMaxValue<float>()));
                inout_parameter.SetValue(std::get<float>(this->widget_store));
            } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                this->widget_store = arg;
            }
        } else if constexpr (std::is_same_v<T, int>) {
            switch (inout_parameter.type) {
            case (Parameter::ParamType::INT): {
                if (!std::holds_alternative<T>(this->widget_store)) {
                    this->widget_store = arg;
                }
                ImGui::InputInt(
                    param_label.c_str(), &std::get<int>(this->widget_store), 1, 10, ImGuiInputTextFlags_None);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    this->widget_store = std::max(inout_parameter.GetMinValue<int>(),
                        std::min(std::get<int>(this->widget_store), inout_parameter.GetMaxValue<int>()));
                    inout_parameter.SetValue(std::get<int>(this->widget_store));
                } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                    this->widget_store = arg;
                }
            } break;
            case (Parameter::ParamType::ENUM): {
                /// XXX: UTF8 conversion and allocation every frame is horrific inefficient.
                auto map = inout_parameter.GetStorage<EnumStorageType>();
                std::string utf8Str = map[arg];
                GUIUtils::Utf8Encode(utf8Str);
                if (ImGui::BeginCombo(param_label.c_str(), utf8Str.c_str())) {
                    for (auto& pair : map) {
                        bool isSelected = (pair.first == arg);
                        utf8Str = pair.second;
                        GUIUtils::Utf8Encode(utf8Str);
                        if (ImGui::Selectable(utf8Str.c_str(), isSelected)) {
                            inout_parameter.SetValue(pair.first);
                        }
                    }
                    ImGui::EndCombo();
                }
            } break;
            default:
                break;
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            switch (inout_parameter.type) {
            case (Parameter::ParamType::STRING): {
                /// XXX: UTF8 conversion and allocation every frame is horrific inefficient.
                if (!std::holds_alternative<T>(this->widget_store)) {
                    std::string utf8Str = arg;
                    GUIUtils::Utf8Encode(utf8Str);
                    this->widget_store = utf8Str;
                }
                // Determine multi line count of string
                int lcnt = static_cast<int>(std::count(std::get<std::string>(this->widget_store).begin(),
                    std::get<std::string>(this->widget_store).end(), '\n'));
                lcnt = std::min(static_cast<int>(GUI_MAX_MULITLINE), lcnt);
                ImVec2 ml_dim = ImVec2(ImGui::CalcItemWidth(),
                    ImGui::GetFrameHeight() + (ImGui::GetFontSize() * static_cast<float>(lcnt)));
                std::string hidden_label = "###" + param_label;
                ImGui::InputTextMultiline(hidden_label.c_str(), &std::get<std::string>(this->widget_store), ml_dim,
                    ImGuiInputTextFlags_CtrlEnterForNewLine);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string utf8Str = std::get<std::string>(this->widget_store);
                    GUIUtils::Utf8Decode(utf8Str);
                    inout_parameter.SetValue(utf8Str);
                } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                    std::string utf8Str = arg;
                    GUIUtils::Utf8Encode(utf8Str);
                    this->widget_store = utf8Str;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(param_label.c_str());
                this->help = "[Ctrl + Enter] for new line.\nPress [Return] to confirm changes.";
            } break;
            case (Parameter::ParamType::TRANSFERFUNCTION): {
                this->transfer_function_edit(inout_parameter);
            } break;
            case (Parameter::ParamType::FILEPATH): {
                /// XXX: UTF8 conversion and allocation every frame is horrific inefficient.
                if (!std::holds_alternative<T>(this->widget_store)) {
                    std::string utf8Str = arg;
                    GUIUtils::Utf8Encode(utf8Str);
                    this->widget_store = utf8Str;
                }
                ImGuiStyle& style = ImGui::GetStyle();
                widget_width -= (ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.x);
                ImGui::PushItemWidth(widget_width);
                bool button_edit = this->file_utils.FileBrowserButton(std::get<std::string>(this->widget_store));
                ImGui::SameLine();
                ImGui::InputText(
                    param_label.c_str(), &std::get<std::string>(this->widget_store), ImGuiInputTextFlags_None);
                if (button_edit || ImGui::IsItemDeactivatedAfterEdit()) {
                    GUIUtils::Utf8Decode(std::get<std::string>(this->widget_store));
                    inout_parameter.SetValue(std::get<std::string>(this->widget_store));
                } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                    std::string utf8Str = arg;
                    GUIUtils::Utf8Encode(utf8Str);
                    this->widget_store = utf8Str;
                }
                ImGui::PopItemWidth();
            } break;
            case (Parameter::ParamType::FLEXENUM): {
                /// XXX: UTF8 conversion and allocation every frame is horrific inefficient.
                if (!std::holds_alternative<T>(this->widget_store)) {
                    this->widget_store = std::string();
                }
                std::string utf8Str = arg;
                GUIUtils::Utf8Encode(utf8Str);
                if (ImGui::BeginCombo(param_label.c_str(), utf8Str.c_str())) {

                    bool one_present = false;
                    for (auto& valueOption :
                        inout_parameter.GetStorage<megamol::core::param::FlexEnumParam::Storage_t>()) {
                        bool isSelected = (valueOption == arg);
                        utf8Str = valueOption;
                        GUIUtils::Utf8Encode(utf8Str);
                        if (ImGui::Selectable(utf8Str.c_str(), isSelected)) {
                            GUIUtils::Utf8Decode(utf8Str);
                            inout_parameter.SetValue(utf8Str);
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        one_present = true;
                    }

                    if (one_present) {
                        ImGui::Separator();
                    }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Add");
                    ImGui::SameLine();
                    /// Keyboard focus needs to be set in/untill second frame
                    if (this->set_focus < 2) {
                        ImGui::SetKeyboardFocusHere();
                        this->set_focus++;
                    }
                    ImGui::InputText(
                        "###flex_enum_text_edit", &std::get<std::string>(this->widget_store), ImGuiInputTextFlags_None);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!std::get<std::string>(this->widget_store).empty()) {
                            GUIUtils::Utf8Decode(std::get<std::string>(this->widget_store));
                            inout_parameter.SetValue(std::get<std::string>(this->widget_store));
                            std::get<std::string>(this->widget_store) = std::string();
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndCombo();
                } else {
                    this->set_focus = 0;
                }

                this->help = "Only selected value will be saved to project file";
            } break;
            default:
                break;
            }
        } else if constexpr (std::is_same_v<T, vislib::math::Ternary>) {
            if (ImGui::RadioButton("True", arg.IsTrue())) {
                inout_parameter.SetValue(vislib::math::Ternary::TRI_TRUE);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("False", arg.IsFalse())) {
                inout_parameter.SetValue(vislib::math::Ternary::TRI_FALSE);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Unknown", arg.IsUnknown())) {
                inout_parameter.SetValue(vislib::math::Ternary::TRI_UNKNOWN);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextUnformatted(param_label.c_str());
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            if (!std::holds_alternative<T>(this->widget_store)) {
                this->widget_store = arg;
            }
            ImGui::InputFloat2(param_label.c_str(), glm::value_ptr(std::get<glm::vec2>(this->widget_store)),
                this->float_format.c_str(), ImGuiInputTextFlags_None);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                auto max = inout_parameter.GetMaxValue<glm::vec2>();
                auto min = inout_parameter.GetMinValue<glm::vec2>();
                auto x = std::max(min.x, std::min(std::get<glm::vec2>(this->widget_store).x, max.x));
                auto y = std::max(min.y, std::min(std::get<glm::vec2>(this->widget_store).y, max.y));
                this->widget_store = glm::vec2(x, y);
                inout_parameter.SetValue(std::get<glm::vec2>(this->widget_store));
            } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                this->widget_store = arg;
            }
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (!std::holds_alternative<T>(this->widget_store)) {
                this->widget_store = arg;
            }
            ImGui::InputFloat3(param_label.c_str(), glm::value_ptr(std::get<glm::vec3>(this->widget_store)),
                this->float_format.c_str(), ImGuiInputTextFlags_None);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                auto max = inout_parameter.GetMaxValue<glm::vec3>();
                auto min = inout_parameter.GetMinValue<glm::vec3>();
                auto x = std::max(min.x, std::min(std::get<glm::vec3>(this->widget_store).x, max.x));
                auto y = std::max(min.y, std::min(std::get<glm::vec3>(this->widget_store).y, max.y));
                auto z = std::max(min.z, std::min(std::get<glm::vec3>(this->widget_store).z, max.z));
                this->widget_store = glm::vec3(x, y, z);
                inout_parameter.SetValue(std::get<glm::vec3>(this->widget_store));
            } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                this->widget_store = arg;
            }
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (!std::holds_alternative<T>(this->widget_store)) {
                this->widget_store = arg;
            }
            ImGui::InputFloat4(param_label.c_str(), glm::value_ptr(std::get<glm::vec4>(this->widget_store)),
                this->float_format.c_str(), ImGuiInputTextFlags_None);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                auto max = inout_parameter.GetMaxValue<glm::vec4>();
                auto min = inout_parameter.GetMinValue<glm::vec4>();
                auto x = std::max(min.x, std::min(std::get<glm::vec4>(this->widget_store).x, max.x));
                auto y = std::max(min.y, std::min(std::get<glm::vec4>(this->widget_store).y, max.y));
                auto z = std::max(min.z, std::min(std::get<glm::vec4>(this->widget_store).z, max.z));
                auto w = std::max(min.w, std::min(std::get<glm::vec4>(this->widget_store).w, max.w));
                this->widget_store = glm::vec4(x, y, z, w);
                inout_parameter.SetValue(std::get<glm::vec4>(this->widget_store));
            } else if (!ImGui::IsItemActive() && !ImGui::IsItemEdited()) {
                this->widget_store = arg;
            }
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            switch (inout_parameter.type) {
            case (Parameter::ParamType::BUTTON): {
                std::string hotkey = "";
                auto keycode = inout_parameter.GetStorage<megamol::core::view::KeyCode>();
                std::string button_hotkey = keycode.ToString();
                if (!button_hotkey.empty()) {
                    hotkey = " (" + button_hotkey + ")";
                }
                param_label += hotkey;
                if (ImGui::Button(param_label.c_str())) {
                    // inout_parameter.setDirty();
                }
            } break;
            default:
                break;
            }
        }
    };

    std::visit(visitor, inout_parameter.GetValue());

    if (this->read_only) {
        GUIUtils::ReadOnlyWigetStyle(false);
    }

    ImGui::PopItemWidth();
}


void megamol::gui::configurator::Parameter::Presentation::present_value_PIN_VALUE_TO_MOUSE(
    megamol::gui::configurator::Parameter& inout_parameter) {

    /// Disabled in Configurator
    /*
        auto hoverFlags = ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenDisabled |
                          ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
        if (!ImGui::IsWindowHovered(hoverFlags)) {

            ImGui::BeginTooltip();

            std::string label = inout_parameter.GetName();
            ImGui::TextDisabled(label.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(inout_parameter.GetValueString().c_str());

            ImGui::EndTooltip();
        }
    */
}


void megamol::gui::configurator::Parameter::Presentation::present_postfix(
    megamol::gui::configurator::Parameter& inout_parameter) {

    this->utils.HoverToolTip(inout_parameter.description, ImGui::GetItemID(), 0.5f);
    this->utils.HelpMarkerToolTip(this->help);
}


void megamol::gui::configurator::Parameter::Presentation::transfer_function_edit(
    megamol::gui::configurator::Parameter& inout_parameter) {

    ImGuiStyle& style = ImGui::GetStyle();

    if ((inout_parameter.type != Parameter::ParamType::TRANSFERFUNCTION) ||
        (!std::holds_alternative<std::string>(inout_parameter.GetValue()))) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Transfer Function Editor is called for incompatible parameter type. [%s, %s, line %d]\n", __FILE__,
            __FUNCTION__, __LINE__);
        return;
    }
    auto value = std::get<std::string>(inout_parameter.GetValue());

    bool updateEditor = false;
    
    ImGui::BeginGroup();

    // Reduced display of value and editor state.
    if (value.empty()) {
        ImGui::TextDisabled("{    (empty)    }");
        ImGui::SameLine();
    } else {
        // Draw texture
        if (this->tf_editor.GetHorizontalTexture() != 0) {
            ImGui::Image(reinterpret_cast<ImTextureID>(this->tf_editor.GetHorizontalTexture()),
                ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImVec4(1.0f, 1.0f, 1.0f, 1.0f), style.Colors[ImGuiCol_Border]);
            ImGui::SameLine(ImGui::CalcItemWidth() + style.ItemInnerSpacing.x);
            ImGui::AlignTextToFramePadding();                
        } else {
            ImGui::TextUnformatted("{ ............. }");
            ImGui::SameLine();
        }
    }
    std::string label = inout_parameter.full_name;
    ImGui::TextUnformatted(label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()));

    ImGui::Indent();

    // Copy transfer function.
    if (ImGui::Button("Copy")) {
#ifdef GUI_USE_GLFW
        auto glfw_win = ::glfwGetCurrentContext();
        ::glfwSetClipboardString(glfw_win, value.c_str());
#elif _WIN32
        ImGui::SetClipboardText(value.c_str());
#else // LINUX
        vislib::sys::Log::DefaultLog.WriteWarn(
            "No clipboard use provided. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Transfer Function JSON String:\n%s", value.c_str());
#endif
    }
    ImGui::SameLine();

    //  Paste transfer function.
    if (ImGui::Button("Paste")) {
#ifdef GUI_USE_GLFW
        auto glfw_win = ::glfwGetCurrentContext();
        inout_parameter.SetValue(std::string(::glfwGetClipboardString(glfw_win)));
#elif _WIN32
        inout_parameter.SetValue(std::string(ImGui::GetClipboardText()));
#else // LINUX
        vislib::sys::Log::DefaultLog.WriteWarn(
            "No clipboard use provided. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
#endif
        updateEditor = true;
    }
    ImGui::SameLine();

    // Edit transfer function.
    if (ImGui::Checkbox("Editor", &this->show_tf_editor)) {
        // Set once
        if (this->show_tf_editor) {
            updateEditor = true;
        }
    }

    ImGui::Unindent();
    ImGui::EndGroup();

    // Propagate the transfer function to the editor.
    if (updateEditor) {
        this->tf_editor.SetTransferFunction(value, false);
    }

    // Draw transfer function editor
    if (this->show_tf_editor) {
        if (this->tf_editor.Draw(false)) {
            std::string value;
            if (this->tf_editor.GetTransferFunction(value)) {
                inout_parameter.SetValue(value);
            }
        }
        ImGui::Separator();
    }
}

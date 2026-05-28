#include "pch.h"
#include "Veridian.h"

namespace Veridian
{
    VSetCtx* VastVeridian = nullptr;
    VSetCtxDtor VastVeridianDtor;

    VSetCtx::VSetCtx(std::filesystem::path Filepath)
    {
        this->Filepath = Filepath;

        if (not std::filesystem::exists(Filepath))
        {
            auto Filename = Filepath.filename();

            Filepath.remove_filename();
            std::filesystem::create_directories(Filepath);

            Filepath /= Filename;

            std::fstream(Filepath.wstring(), std::ios::in | std::ios::out | std::ios::trunc).close();
        }

        FileStream.open(Filepath.wstring(), std::ios::in | std::ios::out);

        if (not FileStream.is_open())
        {
            throw;
        }

        std::string Line;
        std::string CurrentSector = VERIDIAN_FALLBACK_SECTION_NAME;

        while (std::getline(FileStream, Line))
        {
            if (Line.starts_with("["))
            {
                CurrentSector = Line.substr(1, Line.length() - 2);
                continue;
            }
            else if (Line.find("=") != std::string::npos)
            {
                size_t EqualsSign = Line.find("=");

                std::string Name = Line.substr(0, EqualsSign);
                std::string Result = Line.substr(EqualsSign + 1);

                Settings[CurrentSector][Name] = VSetting();

                VSetting& NewSetting = Settings[CurrentSector][Name];

                NewSetting.Name = Name;
                NewSetting.SettingStr = Result;
            }
        }
    }

    VSetCtx::~VSetCtx()
    {
        FileStream.clear();
        FileStream.seekp(0);

        for (auto& Sector : Settings)
        {
            FileStream << "[" << Sector.first << "]\n";

            for (auto& Setting : Settings[Sector.first])
            {
                FileStream << Setting.second.Name << "=" << Setting.second.GetAsString() << "\n";
            }

            FileStream << "\n";
        }

        FileStream.close();
    }

    void InitContext(std::filesystem::path Filepath)
    {
        VastVeridian = new VSetCtx(Filepath);
    }

    void RenderSetting(VSetting& Setting)
    {
        if (Setting.Hidden) return;

        static std::map<VSettingType, ImGuiDataType> VeridianToImGui =
        {
            {VSettingType::VInt, ImGuiDataType_S64},
            {VSettingType::VUInt, ImGuiDataType_U64},
            {VSettingType::VFloat, ImGuiDataType_Float}
        };

        char TempBuffer[513] = "\0";

        if (Setting.Value != nullptr)
        {
            if (Setting.Type == VSettingType::VString)
            {
                strcpy_s(TempBuffer, 513, Setting.Value->String.c_str());
            }

            switch (Setting.Type)
            {
            case VSettingType::VUInt:
            case VSettingType::VInt:
            case VSettingType::VFloat:
                ImGui::InputScalar(Setting.FacingName.c_str(), VeridianToImGui[Setting.Type], Setting.Value);
                break;
            case VSettingType::VBool:
                ImGui::Checkbox(Setting.FacingName.c_str(), &Setting.Value->Bool);
                break;
            case VSettingType::VVec2:
                ImGui::InputFloat2(Setting.FacingName.c_str(), &Setting.Value->Float);
                break;
            case VSettingType::VVec3:
                ImGui::ColorEdit3(Setting.FacingName.c_str(), &Setting.Value->Float);
                break;
            case VSettingType::VVec4:
                ImGui::ColorEdit4(Setting.FacingName.c_str(), &Setting.Value->Float);
                break;
            case VSettingType::VString:
                ImGui::InputText(Setting.FacingName.c_str(), TempBuffer, 513);
                break;
            default:
                ImGui::Text("Error rending setting %s (in %s): unknown type", Setting.Name.c_str(), Setting.Section.c_str());
                break;
            }
        }
        else
        {
            static int64_t Int = 0;
            static uint64_t UInt = 0;
            static float Float = 0;
            static bool Bool = false;
            static VVec2 Vec2 = VVec2();
            static VVec3 Vec3 = VVec3();
            static VVec4 Vec4 = VVec4();
            static std::string String;

            strcpy_s(TempBuffer, 513, String.c_str());

            ImGui::BeginDisabled();

            switch (Setting.Type)
            {
            case VSettingType::VUInt:
                ImGui::InputScalar(Setting.FacingName.c_str(), VeridianToImGui[Setting.Type], &UInt);
                break;
            case VSettingType::VInt:
                ImGui::InputScalar(Setting.FacingName.c_str(), VeridianToImGui[Setting.Type], &Int);
                break;
            case VSettingType::VFloat:
                ImGui::InputScalar(Setting.FacingName.c_str(), VeridianToImGui[Setting.Type], &Float);
                break;
            case VSettingType::VBool:
                ImGui::Checkbox(Setting.FacingName.c_str(), &Bool);
                break;
            case VSettingType::VVec2:
                ImGui::InputFloat2(Setting.FacingName.c_str(), &Vec2.x);
                break;
            case VSettingType::VVec3:
                ImGui::ColorEdit3(Setting.FacingName.c_str(), &Vec3.x);
                break;
            case VSettingType::VVec4:
                ImGui::ColorEdit4(Setting.FacingName.c_str(), &Vec4.x);
                break;
            case VSettingType::VString:
                ImGui::InputText(Setting.FacingName.c_str(), TempBuffer, 513);
                break;
            default:
                ImGui::Text("Error rending setting %s (in %s): unknown type", Setting.Name.c_str(), Setting.Section.c_str());
                break;
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Item has been disabled because it has not been registered properly.");
            }

            ImGui::EndDisabled();
        }
    }

    void RenderSetting(std::string Section, std::string Name)
    {
        if (not VastVeridian->Settings.contains(Section)) return;
        if (not VastVeridian->Settings[Section].contains(Name)) return;

        RenderSetting(VastVeridian->Settings[Section][Name]);
    }

    void RenderSection(std::string Section)
    {
        if (not VastVeridian->Settings.contains(Section)) return;

        for (auto& SetPair : VastVeridian->Settings.at(Section))
        {
            if (SetPair.second.Registered) RenderSetting(SetPair.second);
        }
    }

    void RenderAll(std::string DefaultSectionName)
    {
        static std::string ActiveSection = DefaultSectionName;

        std::map<std::string, size_t> RegisteredSettingsPerSection = {};

        for (auto& SecPair : VastVeridian->Settings)
        {
            for (auto& SetPair : SecPair.second)
            {
                if (SetPair.second.Registered and not SetPair.second.Hidden)
                {
                    RegisteredSettingsPerSection[SecPair.first]++;
                }
            }
        }

        if (ImGui::BeginChild("##SettingsSidebar", { 150.f, 0.f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
        {
            for (auto& SecPair : VastVeridian->Settings)
            {
                if (RegisteredSettingsPerSection[SecPair.first] == 0) continue;

                if (ImGui::Selectable(SecPair.first.c_str(), SecPair.first == ActiveSection))
                {
                    ActiveSection = SecPair.first;
                }
            }

            ImGui::EndChild();
        }

        ImGui::SameLine();

        ImGui::BeginGroup();

        RenderSection(ActiveSection);

        ImGui::EndGroup();
    }
}

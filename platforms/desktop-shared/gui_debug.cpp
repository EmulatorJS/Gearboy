/*
 * Gearboy - Nintendo Game Boy Emulator
 * Copyright (C) 2012  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"
#include "FileBrowser/ImGuiFileBrowser.h"
#include "config.h"
#include "emu.h"
#include "renderer.h"
#include "../../src/gearboy.h"
#include "gui.h"
#include "gui_debug_constants.h"

#define GUI_DEBUG_IMPORT
#include "gui_debug.h"

struct DebugSymbol
{
    int bank;
    u16 address;
    std::string text;
};

struct DisassmeblerLine
{
    bool is_symbol;
    bool is_breakpoint;
    Memory::stDisassembleRecord* record;
    std::string symbol;
};

static MemoryEditor mem_edit;
static ImVec4 cyan = ImVec4(0.0f,1.0f,1.0f,1.0f);
static ImVec4 magenta = ImVec4(1.0f,0.502f,0.957f,1.0f);
static ImVec4 yellow = ImVec4(1.0f,1.0f,0.0f,1.0f);
static ImVec4 red = ImVec4(1.0f,0.149f,0.447f,1.0f);
static ImVec4 green = ImVec4(0.0f,1.0f,0.0f,1.0f);
static ImVec4 white = ImVec4(1.0f,1.0f,1.0f,1.0f);
static ImVec4 gray = ImVec4(0.5f,0.5f,0.5f,1.0f);
static ImVec4 dark_gray = ImVec4(0.1f,0.1f,0.1f,1.0f);
static std::vector<DebugSymbol> symbols;
static Memory::stDisassembleRecord* selected_record = NULL;
static char brk_address[8] = "";

static void debug_window_processor(void);
static void debug_window_io(void);
static void debug_window_audio(void);
static void debug_window_memory(void);
static void debug_window_disassembler(void);
static void debug_window_vram(void);
static void debug_window_vram_background(void);
static void debug_window_vram_tiles(void);
static void debug_window_vram_oam(void);
static void debug_window_vram_palettes(void);
static void add_symbol(const char* line);
static void add_breakpoint(void);
static ImVec4 color_565_to_float(u16 color);

void gui_debug_windows(void)
{
    if (config_debug.debug)
    {
        if (config_debug.show_processor)
            debug_window_processor();
        if (config_debug.show_memory)
            debug_window_memory();
        if (config_debug.show_disassembler)
            debug_window_disassembler();
        if (config_debug.show_iomap)
            debug_window_io();
        if (config_debug.show_audio)
            debug_window_audio();
        if (config_debug.show_video)
            debug_window_vram();

        //ImGui::ShowDemoWindow(&config_debug.debug);
    }
}


void gui_debug_reset(void)
{
    gui_debug_reset_breakpoints();
    gui_debug_reset_symbols();
    selected_record = NULL;
}

void gui_debug_reset_symbols(void)
{
    symbols.clear();
    
    for (int i = 0; i < gui_debug_symbols_count; i++)
        add_symbol(gui_debug_symbols[i]);
}

void gui_debug_load_symbols_file(const char* path)
{
    Log("Loading symbol file %s", path);

    std::ifstream file(path);

    if (file.is_open())
    {
        std::string line;

        while (std::getline(file, line))
        {
            add_symbol(line.c_str());
        }

        file.close();
    }
}

void gui_debug_toggle_breakpoint(void)
{
    if (IsValidPointer(selected_record))
    {
        bool found = false;
        std::vector<Memory::stDisassembleRecord*>* breakpoints = emu_get_core()->GetMemory()->GetBreakpoints();

        for (long unsigned int b = 0; b < breakpoints->size(); b++)
        {
            if ((*breakpoints)[b] == selected_record)
            {
                found = true;
                 InitPointer((*breakpoints)[b]);
                break;
            }
        }

        if (!found)
        {
            breakpoints->push_back(selected_record);
        }
    }
}

void gui_debug_runtocursor(void)
{
    if (IsValidPointer(selected_record))
    {
        emu_get_core()->GetMemory()->SetRunToBreakpoint(selected_record);
        emu_debug_continue();
    }
}

void gui_debug_reset_breakpoints(void)
{
    emu_get_core()->GetMemory()->GetBreakpoints()->clear();
    brk_address[0] = 0;
}

static void debug_window_memory(void)
{
    ImGui::SetNextWindowPos(ImVec2(180, 382), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(482, 308), ImGuiCond_FirstUseEver);

    ImGui::Begin("Memory Editor", &config_debug.show_memory);

    GearboyCore* core = emu_get_core();
    Memory* memory = core->GetMemory();

    ImGui::PushFont(gui_default_font);

    ImGui::TextColored(cyan, "  BANKS: ");ImGui::SameLine();

    ImGui::TextColored(magenta, "ROM1");ImGui::SameLine();
    ImGui::Text("$%02X", memory->GetCurrentRule()->GetCurrentRomBank1Index()); ImGui::SameLine();
    ImGui::TextColored(magenta, "  RAM");ImGui::SameLine();
    ImGui::Text("$%02X", memory->GetCurrentRule()->GetCurrentRamBankIndex()); ImGui::SameLine();
    ImGui::TextColored(magenta, "  WRAM1");ImGui::SameLine();
    ImGui::Text("$%02X", memory->GetCurrentCGBRAMBank()); ImGui::SameLine();
    ImGui::TextColored(magenta, "  VRAM");ImGui::SameLine();
    ImGui::Text("$%02X", memory->GetCurrentLCDRAMBank());

    ImGui::PopFont();

    if (ImGui::BeginTabBar("##memory_tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("ROM0"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetROM0(), 0x4000, 0);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ROM1"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetROM1(), 0x4000, 0x4000);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("VRAM"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetVRAM(), 0x2000, 0x8000);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("RAM"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetRAM(), 0x2000, 0xA000);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (emu_is_cgb())
        {
            if (ImGui::BeginTabItem("WRAM0"))
            {
                ImGui::PushFont(gui_default_font);
                mem_edit.DrawContents(memory->GetWRAM0(), 0x1000, 0xC000);
                ImGui::PopFont();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("WRAM1"))
            {
                ImGui::PushFont(gui_default_font);
                mem_edit.DrawContents(memory->GetWRAM1(), 0x1000, 0xD000);
                ImGui::PopFont();
                ImGui::EndTabItem();
            }
        }
        else
        {
            if (ImGui::BeginTabItem("WRAM"))
            {
                ImGui::PushFont(gui_default_font);
                mem_edit.DrawContents(memory->GetWRAM0(), 0x2000, 0xC000);
                ImGui::PopFont();
                ImGui::EndTabItem();
            }
        }
        
        if (ImGui::BeginTabItem("OAM"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetMemoryMap() + 0xFE00, 0x00A0, 0xFE00);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("IO"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetMemoryMap() + 0xFF00, 0x0080, 0xFF00);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("HIRAM"))
        {
            ImGui::PushFont(gui_default_font);
            mem_edit.DrawContents(memory->GetMemoryMap() + 0xFF80, 0x007F, 0xFF80);
            ImGui::PopFont();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void debug_window_disassembler(void)
{
    ImGui::SetNextWindowPos(ImVec2(180, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(482, 344), ImGuiCond_FirstUseEver);

    ImGui::Begin("Disassembler", &config_debug.show_disassembler);

    GearboyCore* core = emu_get_core();
    Processor* processor = core->GetProcessor();
    Processor::ProcessorState* proc_state = processor->GetState();
    Memory* memory = core->GetMemory();
    std::vector<Memory::stDisassembleRecord*>* breakpoints = memory->GetBreakpoints();
    Memory::stDisassembleRecord* memoryMap = memory->GetDisassembledMemoryMap();
    Memory::stDisassembleRecord* romMap = memory->GetDisassembledROMMemoryMap();
    Memory::stDisassembleRecord* map = NULL;

    int pc = proc_state->PC->GetValue();

    static bool follow_pc = true;
    static bool show_mem = true;
    static bool show_symbols = true;

    ImGui::Checkbox("Follow PC", &follow_pc); ImGui::SameLine();
    ImGui::Checkbox("Show Memory", &show_mem);  ImGui::SameLine();
    ImGui::Checkbox("Show Symbols", &show_symbols);

    if (ImGui::Button("Step Over"))
        emu_debug_step();
    ImGui::SameLine();
    if (ImGui::Button("Step Frame"))
        emu_debug_next_frame();
    ImGui::SameLine();
    if (ImGui::Button("Continue"))
        emu_debug_continue(); 
    ImGui::SameLine();
    if (ImGui::Button("Run To Cursor"))
        gui_debug_runtocursor();

    if (ImGui::CollapsingHeader("Breakpoints"))
    {
        ImGui::Checkbox("Disable All", &emu_debug_disable_breakpoints);

        ImGui::Columns(2, "breakpoints");
        ImGui::SetColumnOffset(1, 85);

        ImGui::Separator();

        if (IsValidPointer(selected_record))
            sprintf(brk_address, "%02X:%04X", selected_record->bank, selected_record->address);
        
        ImGui::PushItemWidth(70);
        ImGui::PushFont(gui_default_font);
        if (ImGui::InputTextWithHint("##add_breakpoint", "XX:XXXX", brk_address, IM_ARRAYSIZE(brk_address), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            add_breakpoint();
        }
        ImGui::PopFont();
        ImGui::PopItemWidth();
        
        if (ImGui::Button("Add", ImVec2(70, 0)))
        {
            add_breakpoint();
        }
        
        if (ImGui::Button("Clear All", ImVec2(70, 0)))
        {
            gui_debug_reset_breakpoints();
        }

        ImGui::NextColumn();

        ImGui::BeginChild("breakpoints", ImVec2(0, 74), false);

        for (long unsigned int b = 0; b < breakpoints->size(); b++)
        {
            if (!IsValidPointer((*breakpoints)[b]))
                continue;

            ImGui::PushID(b);
            if (ImGui::SmallButton("X"))
            {
               InitPointer((*breakpoints)[b]);
               ImGui::PopID();
               continue;
            }

            ImGui::PopID();

            ImGui::PushFont(gui_default_font);
            ImGui::SameLine();
            ImGui::TextColored(red, "%02X:%04X", (*breakpoints)[b]->bank, (*breakpoints)[b]->address);
            ImGui::SameLine();
            ImGui::TextColored(gray, "%s", (*breakpoints)[b]->name);
            ImGui::PopFont();
        }

        ImGui::EndChild();
        ImGui::Columns(1);
        ImGui::Separator();
    }

    ImGui::PushFont(gui_default_font);

    bool window_visible = ImGui::BeginChild("##dis", ImVec2(ImGui::GetWindowContentRegionWidth(), 0), true, 0);
    
    if (window_visible)
    {
        int dis_size = 0;
        int pc_pos = 0;
        
        std::vector<DisassmeblerLine> vec(0x10000);
        
        for (int i = 0; i < 0x10000; i++)
        {
            int offset = i;
            int bank = 0;

            if ((i & 0xC000) == 0x0000)
            {
                bank = memory->GetCurrentRule()->GetCurrentRomBank0Index();
                offset = (0x4000 * bank) + i;
                map = romMap;
            }
            else if ((i & 0xC000) == 0x4000)
            {
                bank = memory->GetCurrentRule()->GetCurrentRomBank1Index();
                offset = (0x4000 * bank) + (i & 0x3FFF);
                map = romMap;
            }
            else
            {
                map = memoryMap;
            }

            if (map[offset].name[0] != 0)
            {
                for (long unsigned int s = 0; s < symbols.size(); s++)
                {
                    if ((symbols[s].bank == bank) && (symbols[s].address == offset) && show_symbols)
                    {
                        vec[dis_size].is_symbol = true;
                        vec[dis_size].symbol = symbols[s].text;
                        dis_size ++;
                    }
                }

                vec[dis_size].is_symbol = false;
                vec[dis_size].record = &map[offset];

                if (vec[dis_size].record->address == pc)
                    pc_pos = dis_size;

                vec[dis_size].is_breakpoint = false;

                for (long unsigned int b = 0; b < breakpoints->size(); b++)
                {
                    if ((*breakpoints)[b] == vec[dis_size].record)
                    {
                        vec[dis_size].is_breakpoint = true;
                        break;
                    }
                }

                dis_size++;
            }
        }

        if (follow_pc)
        {
            float window_offset = ImGui::GetWindowHeight() / 2.0f;
            float offset = window_offset - (ImGui::GetTextLineHeightWithSpacing() - 2.0f);
            ImGui::SetScrollY((pc_pos * ImGui::GetTextLineHeightWithSpacing()) - offset);
        }

        ImGuiListClipper clipper(dis_size, ImGui::GetTextLineHeightWithSpacing());

        while (clipper.Step())
        {
            for (int item = clipper.DisplayStart; item < clipper.DisplayEnd; item++)
            {
                if (vec[item].is_symbol)
                {
                    ImGui::TextColored(green, "%s:", vec[item].symbol.c_str());
                    continue;
                }

                ImGui::PushID(item);

                bool is_selected = (selected_record == vec[item].record);

                if (ImGui::Selectable("", is_selected))
                {
                    if (is_selected)
                    {
                        InitPointer(selected_record);
                        brk_address[0] = 0;
                    }
                    else
                        selected_record = vec[item].record;
                }

                if (is_selected)
                    ImGui::SetItemDefaultFocus();

                if (vec[item].is_breakpoint)
                {
                    ImGui::SameLine();
                    if (vec[item].record->address == pc)
                    {
                        if (show_mem)
                            ImGui::TextColored(red, " %04X: %s", vec[item].record->address, vec[item].record->bytes);
                        else
                            ImGui::TextColored(red, " %04X:", vec[item].record->address);
                        ImGui::SameLine();
                        ImGui::TextColored(yellow, "->");
                        ImGui::SameLine();
                        ImGui::TextColored(red, "%s", vec[item].record->name);
                    }
                    else
                    {
                        if (show_mem)
                            ImGui::TextColored(red, " %04X: %s    %s", vec[item].record->address, vec[item].record->bytes, vec[item].record->name);
                        else
                            ImGui::TextColored(red, " %04X:    %s", vec[item].record->address, vec[item].record->name);
                    }
                } 
                else if (vec[item].record->address == pc)
                {
                    ImGui::SameLine();
                    if (show_mem)
                        ImGui::TextColored(yellow, " %04X: %s -> %s", vec[item].record->address, vec[item].record->bytes, vec[item].record->name);
                    else
                        ImGui::TextColored(yellow, " %04X: -> %s", vec[item].record->address, vec[item].record->name);
                }
                else
                {
                    ImGui::SameLine();
                    ImGui::TextColored(cyan, " %04X:", vec[item].record->address);
                    ImGui::SameLine();
                    if (show_mem)
                        ImGui::TextColored(gray, "%s   ", vec[item].record->bytes);
                    else
                        ImGui::TextColored(gray, "  ");
                    
                    ImGui::SameLine();
                    ImGui::TextColored(white, "%s", vec[item].record->name);
                }

                ImGui::PopID();
            }
        }
    }

    ImGui::EndChild();
    
    ImGui::PopFont();

    ImGui::End();
}

static void debug_window_processor(void)
{
    ImGui::SetNextWindowPos(ImVec2(14, 210), ImGuiCond_FirstUseEver);

    ImGui::Begin("Processor", &config_debug.show_processor, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);

    ImGui::PushFont(gui_default_font);

    GearboyCore* core = emu_get_core();
    Processor* processor = core->GetProcessor();
    Processor::ProcessorState* proc_state = processor->GetState();

    ImGui::Separator();

    u8 flags = proc_state->AF->GetLow();

    ImGui::TextColored(magenta, "   Z"); ImGui::SameLine();
    ImGui::Text("= %d", (bool)(flags & FLAG_ZERO)); ImGui::SameLine();

    ImGui::TextColored(magenta, "  N"); ImGui::SameLine();
    ImGui::Text("= %d", (bool)(flags & FLAG_SUB));

    ImGui::TextColored(magenta, "   H"); ImGui::SameLine();
    ImGui::Text("= %d", (bool)(flags & FLAG_HALF)); ImGui::SameLine();

    ImGui::TextColored(magenta, "  C"); ImGui::SameLine();
    ImGui::Text("= %d", (bool)(flags & FLAG_CARRY));

    ImGui::Columns(2, "registers");
    ImGui::Separator();
    ImGui::TextColored(cyan, " A"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->AF->GetHigh());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->AF->GetHigh()));

    ImGui::NextColumn();
    ImGui::TextColored(cyan, " F"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->AF->GetLow());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->AF->GetLow()));

    ImGui::NextColumn();
    ImGui::Separator();
    ImGui::TextColored(cyan, " B"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->BC->GetHigh());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->BC->GetHigh()));

    ImGui::NextColumn();
    ImGui::TextColored(cyan, " C"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->BC->GetLow());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->BC->GetLow()));

    ImGui::NextColumn();
    ImGui::Separator();
    ImGui::TextColored(cyan, " D"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->DE->GetHigh());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->DE->GetHigh()));

    ImGui::NextColumn();
    ImGui::TextColored(cyan, " E"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->DE->GetLow());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->DE->GetLow()));

    ImGui::NextColumn();
    ImGui::Separator();
    ImGui::TextColored(cyan, " H"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->HL->GetHigh());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->HL->GetHigh()));

    ImGui::NextColumn();
    ImGui::TextColored(cyan, " L"); ImGui::SameLine();
    ImGui::Text("= $%02X", proc_state->HL->GetLow());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->HL->GetLow()));

    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::TextColored(yellow, "    SP"); ImGui::SameLine();
    ImGui::Text("= $%04X", proc_state->SP->GetValue());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED " " BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->SP->GetHigh()), BYTE_TO_BINARY(proc_state->SP->GetLow()));

    ImGui::Separator();
    ImGui::TextColored(yellow, "    PC"); ImGui::SameLine();
    ImGui::Text("= $%04X", proc_state->PC->GetValue());
    ImGui::Text(BYTE_TO_BINARY_PATTERN_SPACED " " BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(proc_state->PC->GetHigh()), BYTE_TO_BINARY(proc_state->PC->GetLow()));


    ImGui::Columns(2);
    ImGui::Separator();

    ImGui::TextColored(magenta, " IME"); ImGui::SameLine();
    ImGui::Text("= %d", *proc_state->IME);

    ImGui::NextColumn();

    ImGui::TextColored(magenta, "HALT"); ImGui::SameLine();
    ImGui::Text("= %d", *proc_state->Halt);

    ImGui::NextColumn();

    ImGui::Columns(1);
    
    ImGui::Separator();

    ImGui::TextColored(cyan, " DOUBLE SPEED "); ImGui::SameLine();
    processor->CGBSpeed() ? ImGui::TextColored(green, "ON") : ImGui::TextColored(gray, "OFF");

    ImGui::PopFont();

    ImGui::End();
}

static void debug_window_audio(void)
{
    ImGui::SetNextWindowPos(ImVec2(130, 264), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(417, 0), ImGuiCond_FirstUseEver);

    ImGui::Begin("Sound Registers", &config_debug.show_audio);

    ImGui::PushFont(gui_default_font);

    GearboyCore* core = emu_get_core();
    Audio* audio = core->GetAudio();

    gb_apu_state_t apu_state;
    audio->GetApu()->save_state(&apu_state);

    ImGui::Columns(2, "audio");

    ImGui::TextColored(yellow, "CHANNEL 1 - TONE & SWEEP:");

    u8 value = apu_state.regs[0xFF10 - 0xFF10];
    ImGui::TextColored(cyan, " $FF10"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR10"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF11 - 0xFF10];
    ImGui::TextColored(cyan, " $FF11"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR11"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF12 - 0xFF10];
    ImGui::TextColored(cyan, " $FF12"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR12"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF13 - 0xFF10];
    ImGui::TextColored(cyan, " $FF13"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR13"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF14 - 0xFF10];
    ImGui::TextColored(cyan, " $FF14"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR14"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    ImGui::NextColumn();

    ImGui::TextColored(yellow, "CHANNEL 3 - WAVE:");

    value = apu_state.regs[0xFF1A - 0xFF10];
    ImGui::TextColored(cyan, " $FF1A"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR30"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF1B - 0xFF10];
    ImGui::TextColored(cyan, " $FF1B"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR31"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF1C - 0xFF10];
    ImGui::TextColored(cyan, " $FF1C"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR32"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF1D - 0xFF10];
    ImGui::TextColored(cyan, " $FF1D"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR33"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF1E - 0xFF10];
    ImGui::TextColored(cyan, " $FF1E"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR34"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    ImGui::NextColumn();
    ImGui::Separator();

    ImGui::TextColored(yellow, "CHANNEL 2 - TONE:");

    value = apu_state.regs[0xFF16 - 0xFF10];
    ImGui::TextColored(cyan, " $FF16"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR21"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF17 - 0xFF10];
    ImGui::TextColored(cyan, " $FF17"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR22"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF18 - 0xFF10];
    ImGui::TextColored(cyan, " $FF18"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR23"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF19 - 0xFF10];
    ImGui::TextColored(cyan, " $FF19"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR24"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    ImGui::NextColumn();

    ImGui::TextColored(yellow, "CHANNEL 4 - NOISE:");

    value = apu_state.regs[0xFF20 - 0xFF10];
    ImGui::TextColored(cyan, " $FF20"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR41"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF21 - 0xFF10];
    ImGui::TextColored(cyan, " $FF21"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR42"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF22 - 0xFF10];
    ImGui::TextColored(cyan, " $FF22"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR43"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF23 - 0xFF10];
    ImGui::TextColored(cyan, " $FF23"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR44"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    ImGui::NextColumn();
    ImGui::Separator();

    ImGui::TextColored(yellow, "CONTROL:");

    value = apu_state.regs[0xFF24 - 0xFF10];
    ImGui::TextColored(cyan, " $FF24"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR50"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF25 - 0xFF10];
    ImGui::TextColored(cyan, " $FF25"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR51"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    value = apu_state.regs[0xFF26 - 0xFF10];
    ImGui::TextColored(cyan, " $FF26"); ImGui::SameLine();
    ImGui::TextColored(magenta, "NR52"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", value, BYTE_TO_BINARY(value));

    ImGui::NextColumn();

    ImGui::TextColored(yellow, "WAVE ($FF30 - $FF37):" );

    ImGui::Text(" %02X%02X %02X%02X %02X%02X %02X%02X", apu_state.regs[0x20], apu_state.regs[0x21], apu_state.regs[0x22], apu_state.regs[0x23], apu_state.regs[0x24], apu_state.regs[0x25], apu_state.regs[0x26], apu_state.regs[0x27]);

    ImGui::TextColored(yellow, "WAVE ($FF38 - $FF3F):" );

    ImGui::Text(" %02X%02X %02X%02X %02X%02X %02X%02X", apu_state.regs[0x28], apu_state.regs[0x29], apu_state.regs[0x2A], apu_state.regs[0x2B], apu_state.regs[0x2C], apu_state.regs[0x2D], apu_state.regs[0x2E], apu_state.regs[0x2F]);

    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::PopFont();

    ImGui::End();
}

static void debug_window_io(void)
{
    ImGui::SetNextWindowPos(ImVec2(121, 164), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);

    ImGui::Begin("IO Map", &config_debug.show_iomap);

    ImGui::PushFont(gui_default_font);

    GearboyCore* core = emu_get_core();
    Memory* memory = core->GetMemory();

    ImGui::Columns(2, "iomap");

    ImGui::TextColored(yellow, "INTERRUPTS:");

    ImGui::TextColored(cyan, " $FFFF"); ImGui::SameLine();
    ImGui::TextColored(magenta, "IE  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFFFF), BYTE_TO_BINARY(memory->Retrieve(0xFFFF)));

    ImGui::TextColored(cyan, " $FF0F"); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF0F), BYTE_TO_BINARY(memory->Retrieve(0xFF0F)));

    ImGui::TextColored(cyan, " VBLNK  "); ImGui::SameLine();
    IsSetBit(memory->Retrieve(0xFF0F), 0) && IsSetBit(memory->Retrieve(0xFFFF), 0) ? ImGui::TextColored(green, "ON  ") : ImGui::TextColored(gray, "OFF "); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFF0F), 0)); ImGui::SameLine();
    ImGui::TextColored(magenta, "  IE:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFFFF), 0));

    ImGui::TextColored(cyan, " STAT   "); ImGui::SameLine();
    IsSetBit(memory->Retrieve(0xFF0F), 1) && IsSetBit(memory->Retrieve(0xFFFF), 1) ? ImGui::TextColored(green, "ON  ") : ImGui::TextColored(gray, "OFF "); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFF0F), 1)); ImGui::SameLine();
    ImGui::TextColored(magenta, "  IE:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFFFF), 1));

    ImGui::TextColored(cyan, " TIMER  "); ImGui::SameLine();
    IsSetBit(memory->Retrieve(0xFF0F), 2) && IsSetBit(memory->Retrieve(0xFFFF), 2) ? ImGui::TextColored(green, "ON  ") : ImGui::TextColored(gray, "OFF "); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFF0F), 2)); ImGui::SameLine();
    ImGui::TextColored(magenta, "  IE:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFFFF), 2));

    ImGui::TextColored(cyan, " SERIAL "); ImGui::SameLine();
    IsSetBit(memory->Retrieve(0xFF0F), 3) && IsSetBit(memory->Retrieve(0xFFFF), 3) ? ImGui::TextColored(green, "ON  ") : ImGui::TextColored(gray, "OFF "); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFF0F), 3)); ImGui::SameLine();
    ImGui::TextColored(magenta, "  IE:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFFFF), 3));

    ImGui::TextColored(cyan, " JOYPAD "); ImGui::SameLine();
    IsSetBit(memory->Retrieve(0xFF0F), 4) && IsSetBit(memory->Retrieve(0xFFFF), 4) ? ImGui::TextColored(green, "ON  ") : ImGui::TextColored(gray, "OFF "); ImGui::SameLine();
    ImGui::TextColored(magenta, "IF:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFF0F), 4)); ImGui::SameLine();
    ImGui::TextColored(magenta, "  IE:"); ImGui::SameLine();
    ImGui::Text("%d", IsSetBit(memory->Retrieve(0xFFFF), 4));

    ImGui::TextColored(yellow, "GBC:");

    ImGui::TextColored(cyan, " $FF4D"); ImGui::SameLine();
    ImGui::TextColored(magenta, "KEY1"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF4D), BYTE_TO_BINARY(memory->Retrieve(0xFF4D)));

    ImGui::TextColored(cyan, " $FF70"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SVBK"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF70), BYTE_TO_BINARY(memory->Retrieve(0xFF70)));

    ImGui::TextColored(yellow, "GBC LCD:");

    ImGui::TextColored(cyan, " $FF68"); ImGui::SameLine();
    ImGui::TextColored(magenta, "BCPS"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF68), BYTE_TO_BINARY(memory->Retrieve(0xFF68)));

    ImGui::TextColored(cyan, " $FF69"); ImGui::SameLine();
    ImGui::TextColored(magenta, "BCPD"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF69), BYTE_TO_BINARY(memory->Retrieve(0xFF69)));

    ImGui::TextColored(cyan, " $FF6A"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OCPS"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF6A), BYTE_TO_BINARY(memory->Retrieve(0xFF6A)));

    ImGui::TextColored(cyan, " $FF6B"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OCPD"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF6B), BYTE_TO_BINARY(memory->Retrieve(0xFF6B)));

    ImGui::TextColored(cyan, " $FF4F"); ImGui::SameLine();
    ImGui::TextColored(magenta, "VBK "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF4F), BYTE_TO_BINARY(memory->Retrieve(0xFF4F)));

    ImGui::TextColored(yellow, "GBC HDMA:");

    ImGui::TextColored(cyan, " $FF51:$FF52"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SOURCE "); ImGui::SameLine();
    ImGui::Text("$%04X", (memory->Retrieve(0xFF51) << 8) | memory->Retrieve(0xFF52));

    ImGui::TextColored(cyan, " $FF53:$FF54"); ImGui::SameLine();
    ImGui::TextColored(magenta, "DEST   "); ImGui::SameLine();
    ImGui::Text("$%04X", (memory->Retrieve(0xFF53) << 8) | memory->Retrieve(0xFF54));

    ImGui::TextColored(cyan, " $FF55"); ImGui::SameLine();
    ImGui::TextColored(magenta, "LEN "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF55), BYTE_TO_BINARY(memory->Retrieve(0xFF55)));

    ImGui::TextColored(yellow, "GBC INFRARED:");

    ImGui::TextColored(cyan, " $FF56"); ImGui::SameLine();
    ImGui::TextColored(magenta, "RP  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF56), BYTE_TO_BINARY(memory->Retrieve(0xFF56)));

    ImGui::NextColumn();

    ImGui::TextColored(yellow, "LCD:");

    ImGui::TextColored(cyan, " $FF40"); ImGui::SameLine();
    ImGui::TextColored(magenta, "LCDC"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF40), BYTE_TO_BINARY(memory->Retrieve(0xFF40)));

    ImGui::TextColored(cyan, " $FF41"); ImGui::SameLine();
    ImGui::TextColored(magenta, "STAT"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF41), BYTE_TO_BINARY(memory->Retrieve(0xFF41)));

    ImGui::TextColored(cyan, " $FF42"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SCY "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF42), BYTE_TO_BINARY(memory->Retrieve(0xFF42)));

    ImGui::TextColored(cyan, " $FF43"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SCX "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF43), BYTE_TO_BINARY(memory->Retrieve(0xFF43)));

    ImGui::TextColored(cyan, " $FF44"); ImGui::SameLine();
    ImGui::TextColored(magenta, "LY  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF44), BYTE_TO_BINARY(memory->Retrieve(0xFF44)));

    ImGui::TextColored(cyan, " $FF45"); ImGui::SameLine();
    ImGui::TextColored(magenta, "LYC "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF45), BYTE_TO_BINARY(memory->Retrieve(0xFF45)));

    ImGui::TextColored(cyan, " $FF46"); ImGui::SameLine();
    ImGui::TextColored(magenta, "DMA "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF46), BYTE_TO_BINARY(memory->Retrieve(0xFF46)));

    ImGui::TextColored(cyan, " $FF47"); ImGui::SameLine();
    ImGui::TextColored(magenta, "BGP "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF47), BYTE_TO_BINARY(memory->Retrieve(0xFF47)));

    ImGui::TextColored(cyan, " $FF48"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OBP0"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF48), BYTE_TO_BINARY(memory->Retrieve(0xFF48)));

    ImGui::TextColored(cyan, " $FF49"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OBP1"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF49), BYTE_TO_BINARY(memory->Retrieve(0xFF49)));

    ImGui::TextColored(cyan, " $FF4A"); ImGui::SameLine();
    ImGui::TextColored(magenta, "WY  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF4A), BYTE_TO_BINARY(memory->Retrieve(0xFF4A)));

    ImGui::TextColored(cyan, " $FF4B"); ImGui::SameLine();
    ImGui::TextColored(magenta, "WX  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF4B), BYTE_TO_BINARY(memory->Retrieve(0xFF4B)));

    ImGui::TextColored(yellow, "TIMER:");

    ImGui::TextColored(cyan, " $FF04"); ImGui::SameLine();
    ImGui::TextColored(magenta, "DIV "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF04), BYTE_TO_BINARY(memory->Retrieve(0xFF04)));

    ImGui::TextColored(cyan, " $FF05"); ImGui::SameLine();
    ImGui::TextColored(magenta, "TIMA"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF05), BYTE_TO_BINARY(memory->Retrieve(0xFF05)));

    ImGui::TextColored(cyan, " $FF06"); ImGui::SameLine();
    ImGui::TextColored(magenta, "TMA "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF06), BYTE_TO_BINARY(memory->Retrieve(0xFF06)));

    ImGui::TextColored(cyan, " $FF07"); ImGui::SameLine();
    ImGui::TextColored(magenta, "TAC "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF07), BYTE_TO_BINARY(memory->Retrieve(0xFF07)));

    ImGui::TextColored(yellow, "INPUT:");

    ImGui::TextColored(cyan, " $FF00"); ImGui::SameLine();
    ImGui::TextColored(magenta, "JOYP"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF00), BYTE_TO_BINARY(memory->Retrieve(0xFF00)));

    ImGui::TextColored(yellow, "SERIAL:");

    ImGui::TextColored(cyan, " $FF01"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SB  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF01), BYTE_TO_BINARY(memory->Retrieve(0xFF01)));

    ImGui::TextColored(cyan, " $FF02"); ImGui::SameLine();
    ImGui::TextColored(magenta, "SC  "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")", memory->Retrieve(0xFF02), BYTE_TO_BINARY(memory->Retrieve(0xFF02)));

    ImGui::Columns(1);

    ImGui::PopFont();

    ImGui::End();
}

static void debug_window_vram(void)
{
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(540, 534), ImGuiCond_FirstUseEver);

    ImGui::Begin("VRAM Viewer", &config_debug.show_video);

    if (ImGui::BeginTabBar("##vram_tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Background"))
        {
            debug_window_vram_background();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tiles"))
        {
            debug_window_vram_tiles();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("OAM"))
        {
            debug_window_vram_oam();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Palettes"))
        {
            debug_window_vram_palettes();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void debug_window_vram_background(void)
{
    Memory* memory = emu_get_core()->GetMemory();

    static bool show_grid = true;
    static bool show_screen = true;
    static int tile_address_radio = 0;
    static int map_address_radio = 0;
    float scale = 1.5f;
    float size = 256.0f * scale;
    float spacing = 8.0f * scale;

    ImGui::Checkbox("Show Grid##grid_bg", &show_grid); ImGui::SameLine();
    ImGui::Checkbox("Show Screen Rect", &show_screen);

    ImGui::PushFont(gui_default_font);

    ImGui::Columns(2, "bg", false);
    ImGui::SetColumnOffset(1, size + 10.0f);

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Image((void*)(intptr_t)renderer_emu_debug_vram_background, ImVec2(size, size));

    if (show_grid)
    {
        float x = p.x;
        for (int n = 0; n <= 32; n++)
        {
            draw_list->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + size), ImColor(dark_gray), 1.0f);
            x += spacing;
        }

        float y = p.y;  
        for (int n = 0; n <= 32; n++)
        {
            draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + size, y), ImColor(dark_gray), 1.0f);
            y += spacing;
        }
    }

    if (show_screen)
    {
        u8 scroll_x = memory->Retrieve(0xFF43);
        u8 scroll_y = memory->Retrieve(0xFF42);

        float grid_x_max = p.x + size;
        float grid_y_max = p.y + size;

        float rect_x_min = p.x + (scroll_x * scale);
        float rect_y_min = p.y + (scroll_y * scale);
        float rect_x_max = p.x + ((scroll_x + GAMEBOY_WIDTH) * scale);
        float rect_y_max = p.y + ((scroll_y + GAMEBOY_HEIGHT) * scale);

        float x_overflow = 0.0f;
        float y_overflow = 0.0f;

        if (rect_x_max > grid_x_max)
            x_overflow = rect_x_max - grid_x_max;
        if (rect_y_max > grid_y_max)
            y_overflow = rect_y_max - grid_y_max;

        draw_list->AddLine(ImVec2(rect_x_min, rect_y_min), ImVec2(fminf(rect_x_max, grid_x_max), rect_y_min), ImColor(green), 2.0f);
        if (x_overflow > 0.0f)
            draw_list->AddLine(ImVec2(p.x, rect_y_min), ImVec2(p.x + x_overflow, rect_y_min), ImColor(green), 2.0f);

        draw_list->AddLine(ImVec2(rect_x_min, rect_y_min), ImVec2(rect_x_min, fminf(rect_y_max, grid_y_max)), ImColor(green), 2.0f);
        if (y_overflow > 0.0f)
            draw_list->AddLine(ImVec2(rect_x_min, p.y), ImVec2(rect_x_min, p.y + y_overflow), ImColor(green), 2.0f);

        draw_list->AddLine(ImVec2(rect_x_min, (y_overflow > 0.0f) ? p.y + y_overflow : rect_y_max), ImVec2(fminf(rect_x_max, grid_x_max), (y_overflow > 0.0f) ? p.y + y_overflow : rect_y_max), ImColor(green), 2.0f);
        if (x_overflow > 0.0f)
            draw_list->AddLine(ImVec2(p.x, (y_overflow > 0.0f) ? p.y + y_overflow : rect_y_max), ImVec2(p.x + x_overflow, (y_overflow > 0.0f) ? p.y + y_overflow : rect_y_max), ImColor(green), 2.0f);

        draw_list->AddLine(ImVec2((x_overflow > 0.0f) ? p.x + x_overflow : rect_x_max, rect_y_min), ImVec2((x_overflow > 0.0f) ? p.x + x_overflow : rect_x_max, fminf(rect_y_max, grid_y_max)), ImColor(green), 2.0f);
        if (y_overflow > 0.0f)
            draw_list->AddLine(ImVec2((x_overflow > 0.0f) ? p.x + x_overflow : rect_x_max, p.y), ImVec2((x_overflow > 0.0f) ? p.x + x_overflow : rect_x_max, p.y + y_overflow), ImColor(green), 2.0f);
    }

    float mouse_x = io.MousePos.x - p.x;
    float mouse_y = io.MousePos.y - p.y;

    int tile_x = -1;
    int tile_y = -1;
    if ((mouse_x >= 0.0f) && (mouse_x < size) && (mouse_y >= 0.0f) && (mouse_y < size))
    {
        tile_x = mouse_x / spacing;
        tile_y = mouse_y / spacing;

        draw_list->AddRect(ImVec2(p.x + (tile_x * spacing), p.y + (tile_y * spacing)), ImVec2(p.x + ((tile_x + 1) * spacing), p.y + ((tile_y + 1) * spacing)), ImColor(cyan), 2.0f, 15, 2.0f);

        ImGui::NextColumn();

        ImGui::Image((void*)(intptr_t)renderer_emu_debug_vram_background, ImVec2(128.0f, 128.0f), ImVec2((1.0f / 32.0f) * tile_x, (1.0f / 32.0f) * tile_y), ImVec2((1.0f / 32.0f) * (tile_x + 1), (1.0f / 32.0f) * (tile_y + 1)));

        ImGui::TextColored(yellow, "DMG:");

        ImGui::TextColored(cyan, " X:"); ImGui::SameLine();
        ImGui::Text("$%02X", tile_x); ImGui::SameLine();
        ImGui::TextColored(cyan, "   Y:"); ImGui::SameLine();
        ImGui::Text("$%02X", tile_y);

        u8 lcdc = memory->Retrieve(0xFF40);

        int tile_start_addr = emu_debug_background_tile_address >= 0 ? emu_debug_background_tile_address : IsSetBit(lcdc, 4) ? 0x8000 : 0x8800;
        int map_start_addr = emu_debug_background_map_address >= 0 ? emu_debug_background_map_address : IsSetBit(lcdc, 3) ? 0x9C00 : 0x9800;

        u16 map_addr = map_start_addr + (32 * tile_y) + tile_x;

        ImGui::TextColored(cyan, " Map Addr: "); ImGui::SameLine();
        ImGui::Text("$%04X", map_addr);

        int map_tile = 0;

        if (tile_start_addr == 0x8800)
        {
            map_tile = static_cast<s8> (memory->Retrieve(map_addr));
            map_tile += 128;
        }
        else
        {
            map_tile = memory->Retrieve(map_addr);
        }

        ImGui::TextColored(cyan, " Tile Addr:"); ImGui::SameLine();
        ImGui::Text("$%04X", tile_start_addr + (map_tile << 4));
        ImGui::TextColored(cyan, " Tile Number:"); ImGui::SameLine();
        ImGui::Text("$%02X", memory->Retrieve(map_addr));

        if (emu_is_cgb())
        {
            ImGui::TextColored(yellow, "GBC:");

            u8 cgb_tile_attr = memory->ReadCGBLCDRAM(map_addr, true);
            int cgb_tile_pal = cgb_tile_attr & 0x07;
            int cgb_tile_bank = IsSetBit(cgb_tile_attr, 3) ? 1 : 0;
            bool cgb_tile_xflip = IsSetBit(cgb_tile_attr, 5);
            bool cgb_tile_yflip = IsSetBit(cgb_tile_attr, 6);

            ImGui::TextColored(cyan, " Attributes:"); ImGui::SameLine();
            ImGui::Text("$%02X", cgb_tile_attr);
            ImGui::TextColored(cyan, " Palette:"); ImGui::SameLine();
            ImGui::Text("%d", cgb_tile_pal);
            ImGui::TextColored(cyan, " Bank:"); ImGui::SameLine();
            ImGui::Text("%d", cgb_tile_bank);

            ImGui::TextColored(cyan, " X-Flip:"); ImGui::SameLine();
            cgb_tile_xflip ? ImGui::TextColored(green, "ON") : ImGui::TextColored(gray, "OFF");

            ImGui::TextColored(cyan, " Y-Flip:"); ImGui::SameLine();
            cgb_tile_yflip ? ImGui::TextColored(green, "ON") : ImGui::TextColored(gray, "OFF");
        }
    }

    ImGui::Columns(1);

    ImGui::PopFont();

    ImGui::Text("Tile address:"); ImGui::SameLine();
    ImGui::RadioButton("Auto##tile", &tile_address_radio, 0); ImGui::SameLine();
    ImGui::RadioButton("0x8000", &tile_address_radio, 1); ImGui::SameLine();
    ImGui::RadioButton("0x8800", &tile_address_radio, 2);

    switch (tile_address_radio)
    {
    case 0:
        emu_debug_background_tile_address = -1;
        break;
    case 1:
        emu_debug_background_tile_address = 0x8000;
        break;
    case 2:
        emu_debug_background_tile_address = 0x8800;
        break;
    default:
        emu_debug_background_tile_address = -1;
        break;
    }

    ImGui::Text("Map address:"); ImGui::SameLine();
    ImGui::RadioButton("Auto##map", &map_address_radio, 0); ImGui::SameLine();
    ImGui::RadioButton("0x9C00", &map_address_radio, 1); ImGui::SameLine();
    ImGui::RadioButton("0x9800", &map_address_radio, 2);

    switch (map_address_radio)
    {
    case 0:
        emu_debug_background_map_address = -1;
        break;
    case 1:
        emu_debug_background_map_address = 0x9C00;
        break;
    case 2:
        emu_debug_background_map_address = 0x9800;
        break;
    default:
        emu_debug_background_map_address = -1;
        break;
    }
}

static void debug_window_vram_tiles(void)
{
    static bool show_grid = true;
    float scale = 1.5f;
    float width = 8.0f * 16.0f * scale;
    float height = 8.0f * 24.0f * scale;
    float spacing = 8.0f * scale;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 p[2];

    ImGui::Checkbox("Show Grid##grid_tiles", &show_grid);
    ImGui::SameLine(150.0f);

    ImGui::PushItemWidth(80.0f);

    if (!emu_is_cgb())
    {
        ImGui::Combo("Palette##dmg_tile_palette", &emu_debug_tile_dmg_palette, "BGP\0OBP0\0OBP1\0\0");
    }
    else
    {
        ImGui::Combo("Palette##cgb_tile_palette", &emu_debug_tile_color_palette, "BCP0\0BCP1\0BCP2\0BCP3\0BCP4\0BCP5\0BCP6\0BCP7\0OCP0\0OCP1\0OCP2\0OCP3\0OCP4\0OCP5\0OCP6\0OCP7\0\0");
    }

    ImGui::PopItemWidth();

    ImGui::Columns(2, "bg", false);
    ImGui::SetColumnOffset(1, (width * 2.0f) + 16.0f);

    p[0] = ImGui::GetCursorScreenPos();
    
    ImGui::Image((void*)(intptr_t)renderer_emu_debug_vram_tiles[0], ImVec2(width, height));

    ImGui::SameLine();

    p[1] = ImGui::GetCursorScreenPos();

    ImGui::Image((void*)(intptr_t)renderer_emu_debug_vram_tiles[1], ImVec2(width, height));

    for (int i = 0; i < 2; i++)
    {
        if (show_grid)
        {
            float x = p[i].x;
            for (int n = 0; n <= 16; n++)
            {
                draw_list->AddLine(ImVec2(x, p[i].y), ImVec2(x, p[i].y + height), ImColor(dark_gray), 1.0f);
                x += spacing;
            }

            float y = p[i].y;  
            for (int n = 0; n <= 24; n++)
            {
                draw_list->AddLine(ImVec2(p[i].x, y), ImVec2(p[i].x + width, y), ImColor(dark_gray), ((n == 8) || (n == 16)) ? 3.0f : 1.0f);
                y += spacing;
            }
        }
    }

    for (int i = 0; i < 2; i++)
    {
        float mouse_x = io.MousePos.x - p[i].x;
        float mouse_y = io.MousePos.y - p[i].y;

        int tile_x = -1;
        int tile_y = -1;

        if ((mouse_x >= 0.0f) && (mouse_x < width) && (mouse_y >= 0.0f) && (mouse_y < height))
        {
            tile_x = mouse_x / spacing;
            tile_y = mouse_y / spacing;

            draw_list->AddRect(ImVec2(p[i].x + (tile_x * spacing), p[i].y + (tile_y * spacing)), ImVec2(p[i].x + ((tile_x + 1) * spacing), p[i].y + ((tile_y + 1) * spacing)), ImColor(cyan), 2.0f, 15, 2.0f);

            ImGui::NextColumn();

            ImGui::Image((void*)(intptr_t)renderer_emu_debug_vram_tiles[i], ImVec2(128.0f, 128.0f), ImVec2((1.0f / 16.0f) * tile_x, (1.0f / 24.0f) * tile_y), ImVec2((1.0f / 16.0f) * (tile_x + 1), (1.0f / 24.0f) * (tile_y + 1)));

            ImGui::PushFont(gui_default_font);

            ImGui::TextColored(yellow, "DETAILS:");

            int tile_full = (tile_y << 4) + tile_x;
            int tile = tile_full & 0xFF;

            ImGui::TextColored(cyan, " Tile Number:"); ImGui::SameLine();
            ImGui::Text("$%02X", tile); 
            ImGui::TextColored(cyan, " Tile Addr:"); ImGui::SameLine();
            ImGui::Text("$%04X", 0x8000 + (tile_full << 4)); 

            ImGui::PopFont();
        }
    }

    ImGui::Columns(1);
}

static void debug_window_vram_oam(void)
{
    ImGui::PushFont(gui_default_font);
    ImGui::TextColored(cyan, "oam"); 
    ImGui::PopFont();
}

static void debug_window_vram_palettes(void)
{
    GearboyCore* core = emu_get_core();
    Video* video = core->GetVideo();
    Memory* memory = core->GetMemory();
    u16* palette = core->GetDMGInternalPalette();

    ImGui::PushFont(gui_default_font);

    ImGui::TextColored(yellow, "DMG:"); ImGui::SameLine();
    ImGui::TextColored(cyan, "                          0       1       2       3");

    u8 bgp = memory->Retrieve(0xFF47);
    u8 obp0 = memory->Retrieve(0xFF48);
    u8 obp1 = memory->Retrieve(0xFF49);
    
    ImGui::TextColored(cyan, " $FF47"); ImGui::SameLine();
    ImGui::TextColored(magenta, "BGP "); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")  ", bgp, BYTE_TO_BINARY(bgp)); ImGui::SameLine();

    for (int i = 0; i < 4; i++)
    {
        int index = (bgp >> (i * 2)) & 0x03;
        int color = palette[index];
        ImVec4 float_color = color_565_to_float(color);
        char id[16];
        sprintf(id, "##dmg_bg_%d", i);
        ImGui::ColorEdit3(id, (float*)&float_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker); ImGui::SameLine();
        ImGui::Text("%d  ", index);
        if (i < 3)
            ImGui::SameLine();
    }

    ImGui::TextColored(cyan, " $FF48"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OBP0"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")  ", obp0, BYTE_TO_BINARY(obp0)); ImGui::SameLine();

    for (int i = 0; i < 4; i++)
    {
        int index = (obp0 >> (i * 2)) & 0x03;
        int color = palette[index];
        ImVec4 float_color = color_565_to_float(color);
        char id[16];
        sprintf(id, "##dmg_bg_%d", i);
        ImGui::ColorEdit3(id, (float*)&float_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker); ImGui::SameLine();
        ImGui::Text("%d  ", index);
        if (i < 3)
            ImGui::SameLine();
    }

    ImGui::TextColored(cyan, " $FF49"); ImGui::SameLine();
    ImGui::TextColored(magenta, "OBP1"); ImGui::SameLine();
    ImGui::Text("$%02X (" BYTE_TO_BINARY_PATTERN_SPACED ")  ", obp1, BYTE_TO_BINARY(obp1)); ImGui::SameLine();

    for (int i = 0; i < 4; i++)
    {
        int index = (obp1 >> (i * 2)) & 0x03;
        int color = palette[index];
        ImVec4 float_color = color_565_to_float(color);
        char id[16];
        sprintf(id, "##dmg_bg_%d", i);
        ImGui::ColorEdit3(id, (float*)&float_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker); ImGui::SameLine();
        ImGui::Text("%d  ", index);
        if (i < 3)
            ImGui::SameLine();
    }

    ImGui::Text(" ");

    PaletteMatrix bg_palettes = video->GetCGBBackgroundPalettes();
    PaletteMatrix sprite_palettes = video->GetCGBSpritePalettes();

    ImGui::Columns(2, "palettes");

    ImGui::TextColored(yellow, "GBC BACKGROUND:");

    ImGui::NextColumn();

    ImGui::TextColored(yellow, "GBC SPRITES:");

    ImGui::NextColumn();

    ImGui::Separator();

    for (int p = 0; p < 8; p++)
    {
        ImGui::TextColored(cyan, " %d ", p); ImGui::SameLine();

        for (int c = 0; c < 4; c++)
        {
            u16 color = (*bg_palettes)[p][c][1];
            ImVec4 float_color = color_565_to_float(color);
            char id[16];
            sprintf(id, "##cgb_bg_%d_%d", p, c);
            ImGui::ColorEdit3(id, (float*)&float_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
            if (c < 3)
            {   
                ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
            }
        }

        ImGui::Text("  "); ImGui::SameLine();

        for (int c = 0; c < 4; c++)
        {
            u16 color = (*bg_palettes)[p][c][1];
            ImGui::Text("%04X ", color);
            if (c < 3)
                ImGui::SameLine();
        }
    }

    ImGui::NextColumn();

    for (int p = 0; p < 8; p++)
    {
        ImGui::TextColored(cyan, " %d ", p); ImGui::SameLine();

        for (int c = 0; c < 4; c++)
        {
            u16 color = (*sprite_palettes)[p][c][1];
            ImVec4 float_color = color_565_to_float(color);
            char id[16];
            sprintf(id, "##cgb_bg_%d_%d", p, c);
            ImGui::ColorEdit3(id, (float*)&float_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
            if (c < 3)
            {
                ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
            }
        }

        ImGui::Text("  "); ImGui::SameLine();

        for (int c = 0; c < 4; c++)
        {
            u16 color = (*sprite_palettes)[p][c][1];
            ImGui::Text("%04X ", color);
            if (c < 3)
                ImGui::SameLine();
        }
    }

    ImGui::Columns(1);

    ImGui::PopFont();
}

static void add_symbol(const char* line)
{
    Log("Loading symbol %s", line);

    DebugSymbol s;

    std::string str(line);

    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());

    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first)
    {
        str = "";
    }
    else
    {
        size_t last = str.find_last_not_of(' ');
        str = str.substr(first, (last - first + 1));
    }

    std::size_t comment = str.find(";");

    if (comment != std::string::npos)
        str = str.substr(0 , comment);

    std::size_t space = str.find(" ");

    if (space != std::string::npos)
    {
        s.text = str.substr(space + 1 , std::string::npos);
        str = str.substr(0, space);

        std::size_t separator = str.find(":");

        if (separator != std::string::npos)
        {
            s.address = std::stoul(str.substr(separator + 1 , std::string::npos), 0, 16);

            s.bank = std::stoul(str.substr(0, separator), 0 , 16);
        }
        else
        {
            s.address = std::stoul(str, 0, 16);
            s.bank = 0;
        }

        symbols.push_back(s);
    }
}

static void add_breakpoint(void)
{
    int input_len = strlen(brk_address);
    u16 target_address = 0;
    int target_bank = 0;
    int target_offset = 0;

    if ((input_len == 7) && (brk_address[2] == ':'))
    {
        std::string str(brk_address);   
        std::size_t separator = str.find(":");

        if (separator != std::string::npos)
        {
            target_address = std::stoul(str.substr(separator + 1 , std::string::npos), 0, 16);

            target_bank = std::stoul(str.substr(0, separator), 0 , 16);
            target_bank &= 0xFF;
        }
    } 
    else if (input_len == 4)
    {
        target_bank = 0; 
        target_address = std::stoul(brk_address, 0, 16);
    }
    else
    {
        return;
    }

    Memory::stDisassembleRecord* memoryMap = emu_get_core()->GetMemory()->GetDisassembledMemoryMap();
    Memory::stDisassembleRecord* romMap = emu_get_core()->GetMemory()->GetDisassembledROMMemoryMap();
    Memory::stDisassembleRecord* map = NULL;

    if ((target_address & 0xC000) == 0x0000)
    {
        target_offset = (0x4000 * target_bank) + target_address;
        map = romMap;
    }
    else if ((target_address & 0xC000) == 0x4000)
    {
        target_offset = (0x4000 * target_bank) + (target_address & 0x3FFF);
        map = romMap;
    }
    else
    {
        map = memoryMap;
    }

    brk_address[0] = 0;

    bool found = false;
    std::vector<Memory::stDisassembleRecord*>* breakpoints = emu_get_core()->GetMemory()->GetBreakpoints();

    for (long unsigned int b = 0; b < breakpoints->size(); b++)
    {
        if ((*breakpoints)[b] == &map[target_offset])
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        breakpoints->push_back(&map[target_offset]);
    }
}

static ImVec4 color_565_to_float(u16 color)
{
    ImVec4 ret;
    ret.w = 0;
    ret.x = (1.0f / 31.0f) * ((color >> 11) & 0x1F);
    ret.y = (1.0f / 63.0f) * ((color >> 5) & 0x3F);
    ret.z = (1.0f / 31.0f) * (color & 0x1F);
    return ret;
}

// BSD 3-Clause License
//
// Copyright (c) 2022, Arm Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <Windows.h>

#include "wperf.h"
#include "debug.h"
#include "wperf-common\public.h"
#include "wperf-common\macros.h"
#include "wperf-common\iorequest.h"
#include "utils.h"
#include "output.h"
#include "exception.h"
#include "pe_file.h"
#include "process_api.h"
#include "events.h"
#include "pmu_device.h"
#include "user_request.h"
#include "config.h"
#include "perfdata.h"

static bool no_ctrl_c = true;

static BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        no_ctrl_c = false;
        m_out.GetOutputStream() << L"Ctrl-C received, quit counting...";
        return TRUE;
    case CTRL_BREAK_EVENT:
    default:
        m_out.GetErrorOutputStream() << L"unsupported dwCtrlType " << dwCtrlType << std::endl;
        return FALSE;
    }
}


int __cdecl
wmain(
    _In_ const int argc,
    _In_reads_(argc) const wchar_t* argv[]
)
{
    auto exit_code = EXIT_SUCCESS;

    user_request request;
    pmu_device pmu_device;
    wstr_vec raw_args;

    try {
        pmu_device.init();
    }
    catch (std::exception&) {
        exit_code = EXIT_FAILURE;
        goto clean_exit;
    }

    for (int i = 1; i < argc; i++)
        raw_args.push_back(argv[i]);

    try
    {
        struct pmu_device_cfg pmu_cfg;
        pmu_device.get_pmu_device_cfg(pmu_cfg);
        request.init(raw_args, pmu_cfg, pmu_device.builtin_metrics, pmu_events::extra_events);
        pmu_device.do_verbose = request.do_verbose;
    }
    catch (std::exception&)
    {
        exit_code = EXIT_FAILURE;
        goto clean_exit;
    }

    if (request.do_help)
    {
        user_request::print_help();
        goto clean_exit;
    }

    uint32_t enable_bits = 0;
    try
    {
        std::vector<enum evt_class> e_classes;
        for (const auto& [key, _] : request.ioctl_events)
            e_classes.push_back(key);

        enable_bits = pmu_device.enable_bits(e_classes);
    }
    catch (fatal_exception& e)
    {
        m_out.GetErrorOutputStream() << e.what() << std::endl;
        exit_code = EXIT_FAILURE;
        goto clean_exit;
    }

    if (request.do_version)
    {
        version_info driver_ver;
        pmu_device.do_version(driver_ver);

        if (driver_ver.major != MAJOR || driver_ver.minor != MINOR
            || driver_ver.patch != PATCH)
        {
            m_out.GetErrorOutputStream() << L"Version mismatch between wperf-driver and wperf.\n";
            m_out.GetErrorOutputStream() << L"wperf-driver version: " << driver_ver.major << "."
                << driver_ver.minor << "." << driver_ver.patch << "\n";
            m_out.GetErrorOutputStream() << L"wperf version: " << MAJOR << "." << MINOR << "."
                << PATCH << "\n";
            exit_code = EXIT_FAILURE;
        }

        goto clean_exit;
    }

    if (request.do_test)
    {
        pmu_device.do_test(enable_bits, request.ioctl_events);
        goto clean_exit;
    }

    try
    {
        if (request.do_list)
        {
            pmu_device.do_list(request.metrics);
            goto clean_exit;
        }

        pmu_device.post_init(request.cores_idx, request.dmc_idx, request.do_timeline, enable_bits);

        if (request.do_count)
        {
            if (!request.has_events())
            {
                m_out.GetErrorOutputStream() << "no event specified\n";
                return -1;
            }
            else if (request.do_verbose)
            {
                request.show_events();
            }

            if (SetConsoleCtrlHandler(&ctrl_handler, TRUE) == FALSE)
                throw fatal_exception("SetConsoleCtrlHandler failed");

            uint32_t stop_bits = pmu_device.stop_bits();

            pmu_device.stop(stop_bits);

            pmu_device.timeline_params(request.ioctl_events, request.count_interval, request.do_kernel);

            for (uint32_t core_idx : request.cores_idx)
                pmu_device.events_assign(core_idx, request.ioctl_events, request.do_kernel);

            pmu_device.timeline_header(request.ioctl_events);

            int64_t counting_duration_iter = request.count_duration > 0 ?
                static_cast<int64_t>(request.count_duration * 10) : _I64_MAX;

            int64_t counting_interval_iter = request.count_interval > 0 ?
                static_cast<int64_t>(request.count_interval * 2) : 0;

            int counting_timeline_times = request.count_timeline;

            do
            {
                pmu_device.reset(enable_bits);

                SYSTEMTIME timestamp_a;
                GetSystemTime(&timestamp_a);

                pmu_device.start(enable_bits);

                m_out.GetOutputStream() << L"counting ... -";

                int progress_map_index = 0;
                wchar_t progress_map[] = { L'/', L'|', L'\\', L'-' };
                int64_t t_count1 = counting_duration_iter;

                while (t_count1 > 0 && no_ctrl_c)
                {
                    m_out.GetOutputStream() << L'\b' << progress_map[progress_map_index % 4];
                    t_count1--;
                    Sleep(100);
                    progress_map_index++;
                }
                m_out.GetOutputStream() << L'\b' << "done\n";

                pmu_device.stop(enable_bits);

                SYSTEMTIME timestamp_b;
                GetSystemTime(&timestamp_b);

                if (enable_bits & CTL_FLAG_CORE)
                {
                    pmu_device.core_events_read();
                    pmu_device.print_core_stat(request.ioctl_events[EVT_CORE]);
                    pmu_device.print_core_metrics(request.ioctl_events[EVT_CORE]);
                }

                if (enable_bits & CTL_FLAG_DSU)
                {
                    pmu_device.dsu_events_read();
                    pmu_device.print_dsu_stat(request.ioctl_events[EVT_DSU], request.report_l3_cache_metric);
                }

                if (enable_bits & CTL_FLAG_DMC)
                {
                    pmu_device.dmc_events_read();
                    pmu_device.print_dmc_stat(request.ioctl_events[EVT_DMC_CLK], request.ioctl_events[EVT_DMC_CLKDIV2], request.report_ddr_bw_metric);
                }

                ULARGE_INTEGER li_a, li_b;
                FILETIME time_a, time_b;

                SystemTimeToFileTime(&timestamp_a, &time_a);
                SystemTimeToFileTime(&timestamp_b, &time_b);
                li_a.u.LowPart = time_a.dwLowDateTime;
                li_a.u.HighPart = time_a.dwHighDateTime;
                li_b.u.LowPart = time_b.dwLowDateTime;
                li_b.u.HighPart = time_b.dwHighDateTime;

                if (!request.do_timeline)
                {
                    double duration = (double)(li_b.QuadPart - li_a.QuadPart) / 10000000.0;
                    m_out.GetOutputStream() << std::endl;
                    m_out.GetOutputStream() << std::right << std::setw(20)
                        << duration << L" seconds time elapsed" << std::endl;
                    m_globalJSON.m_duration = duration;
                }
                else
                {
                    m_out.GetOutputStream() << L"sleeping ... -";
                    int64_t t_count2 = counting_interval_iter;
                    for (; t_count2 > 0 && no_ctrl_c; t_count2--)
                    {
                        m_out.GetOutputStream() << L'\b' << progress_map[t_count2 % 4];
                        Sleep(500);
                    }

                    m_out.GetOutputStream() << L'\b' << "done\n";

                }

                if (m_outputType == TableType::JSON || m_outputType == TableType::ALL)
                {
                    m_out.Print(m_globalJSON);
                }

                if (counting_timeline_times > 0)
                {
                    --counting_timeline_times;
                    if (counting_timeline_times <= 0)
                        break;
                }
            } while (request.do_timeline && no_ctrl_c);
        }
        else if (request.do_sample)
        {
            PerfDataWriter perfDataWriter;
            if (request.do_export_perf_data)
                perfDataWriter.WriteCommandLine(argc, argv);

            if (SetConsoleCtrlHandler(&ctrl_handler, TRUE) == FALSE)
                throw fatal_exception("SetConsoleCtrlHandler failed for sampling");

            if (request.sample_pe_file == L"")
                throw fatal_exception("PE file not specified");

            if (request.sample_pdb_file == L"")
                throw fatal_exception("PDB file not specified");

            if (request.cores_idx.size() > 1)
                throw fatal_exception("you can specify only one core for sampling");

            std::vector<SectionDesc> sec_info;          // List of sections in executable
            std::vector<FuncSymDesc> sym_info;
            std::vector<std::wstring> sec_import;       // List of DLL imported by executable
            uint64_t static_entry_point, image_base;

            parse_pe_file(request.sample_pe_file, static_entry_point, image_base, sec_info, sec_import);
            parse_pdb_file(request.sample_pdb_file, sym_info, request.sample_display_short);

            uint32_t stop_bits = CTL_FLAG_CORE;

            pmu_device.stop(stop_bits);

            pmu_device.set_sample_src(request.ioctl_events_sample, request.do_kernel);

            UINT64 runtime_vaddr_delta = 0;

            std::map<std::wstring, PeFileMetaData> dll_metadata;        // [pe_name] -> PeFileMetaData
            std::map<std::wstring, ModuleMetaData> modules_metadata;    // [mod_name] -> ModuleMetaData

            HMODULE hMods[1024];
            DWORD cbNeeded;
            DWORD pid = FindProcess(request.sample_image_name);
            HANDLE process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, 0, pid);

            if (request.do_export_perf_data)
            {
                perfDataWriter.RegisterEvent(PerfDataWriter::COMM, pid, request.sample_image_name);
            }

            if (EnumProcessModules(process_handle, hMods, sizeof(hMods), &cbNeeded))
            {
                for (auto i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
                {
                    TCHAR szModName[MAX_PATH];
                    TCHAR lpszBaseName[MAX_PATH];

                    std::wstring name;
                    if (GetModuleBaseNameW(process_handle, hMods[i], lpszBaseName, MAX_PATH))
                    {
                        name = lpszBaseName;
                        modules_metadata[name].mod_name = name;
                    }

                    // Get the full path to the module's file.
                    if (GetModuleFileNameEx(process_handle, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
                    {
                        std::wstring mod_path = szModName;
                        modules_metadata[name].mod_path = mod_path;
                        modules_metadata[name].handle = hMods[i];

                        MODULEINFO modinfo;
                        if (GetModuleInformation(process_handle, hMods[i], &modinfo, sizeof(MODULEINFO)))
                        {
                            if (request.do_export_perf_data)
                            {
                                perfDataWriter.RegisterEvent(PerfDataWriter::MMAP, pid, reinterpret_cast<UINT64>(modinfo.lpBaseOfDll), modinfo.SizeOfImage, mod_path, 0);
                            }
                        }
                        else {
                            m_out.GetErrorOutputStream() << "Failed to get module " << szModName << " information" << std::endl;
                        }

                    }
                }
            }

            if (request.do_verbose)
            {
                m_out.GetOutputStream() << L"================================" << std::endl;
                for (const auto& [key, value] : modules_metadata)
                {
                    m_out.GetOutputStream() << std::setw(32) << key
                        << std::setw(32) << IntToHexWideString((ULONGLONG)value.handle, 20)
                        << L"          " << value.mod_path << std::endl;
                }
            }

            for (auto& [key, value] : modules_metadata)
            {
                std::wstring pdb_path = gen_pdb_name(value.mod_path);
                std::ifstream ifile(pdb_path);
                if (ifile) {
                    PeFileMetaData pefile_metadata;
                    parse_pe_file(value.mod_path, pefile_metadata);
                    dll_metadata[value.mod_name] = pefile_metadata;

                    parse_pdb_file(pdb_path, value.sym_info, request.sample_display_short);
                    ifile.close();
                }
            }

            if (request.do_verbose)
            {
                m_out.GetOutputStream() << L"================================" << std::endl;
                for (const auto& [key, value] : dll_metadata)
                {
                    m_out.GetOutputStream() << std::setw(32) << key
                        << L"          " << value.pe_name << std::endl;

                    for (auto& sec : value.sec_info)
                    {
                        m_out.GetOutputStream() << std::setw(32) << sec.name
                            << std::setw(32) << IntToHexWideString(sec.offset, 20)
                            << std::setw(32) << IntToHexWideString(sec.virtual_size)
                            << std::endl;
                    }
                }
            }

            HMODULE module_handle = GetModule(process_handle, request.sample_image_name);
            MODULEINFO modinfo;
            bool ret = GetModuleInformation(process_handle, module_handle, &modinfo, sizeof(MODULEINFO));
            if (!ret)
            {
                m_out.GetOutputStream() << L"failed to query base address of '" << request.sample_image_name << L"'\n";
            }
            else
            {
                runtime_vaddr_delta = (UINT64)modinfo.EntryPoint - (image_base + static_entry_point);
                m_out.GetOutputStream() << L"base address of '" << request.sample_image_name
                    << L"': 0x" << std::hex << (UINT64)modinfo.EntryPoint
                    << L", runtime delta: 0x" << runtime_vaddr_delta << std::endl;
            }
            
            std::vector<FrameChain> raw_samples;
            {
                DWORD image_exit_code = 0;

                int64_t sampling_duration_iter = request.count_duration > 0 ?
                    static_cast<int64_t>(request.count_duration * 10) : _I64_MAX;
                int64_t t_count1 = sampling_duration_iter;

                pmu_device.start_sample();

                m_out.GetOutputStream() << L"sampling ...";

                do
                {
                    t_count1--;
                    Sleep(100);

                    if ((t_count1 % 10) == 0)
                    {
                        if (pmu_device.get_sample(raw_samples))
                            m_out.GetOutputStream() << L".";
                        else
                            m_out.GetOutputStream() << L"e";
                    }

                    if (GetExitCodeProcess(process_handle, &image_exit_code))
                        if (image_exit_code != STILL_ACTIVE)
                            break;
                }
                while (t_count1 > 0 && no_ctrl_c);

                m_out.GetOutputStream() << " done!" << std::endl;

                pmu_device.stop_sample();

                if (request.do_verbose)
                    m_out.GetOutputStream() << "Sampling stopped, process pid=" << pid
                    << L" exited with code " << IntToHexWideString(image_exit_code) << std::endl;
            }

            CloseHandle(process_handle);
            
            std::vector<SampleDesc> resolved_samples;

            for (const auto& a : raw_samples)
            {
                bool found = false;
                SampleDesc sd;
                uint64_t sec_base = 0;

                // Search in symbol table for image (executable)
                for (const auto& b : sym_info)
                {
                    for (const auto& c : sec_info)
                    {
                        if (c.idx == (b.sec_idx - 1))
                        {
                            sec_base = image_base + c.offset + runtime_vaddr_delta;
                            break;
                        }
                    }

                    if (a.pc >= (b.offset + sec_base) && a.pc < (b.offset + sec_base + b.size))
                    {
                        sd.desc = b;
                        sd.module = 0;
                        found = true;
                        break;
                    }
                }

                // Nothing was found in base images, let's search inside modules loaded with
                // images (such as DLLs).
                // Note: at this point:
                //  `dll_metadata` contains names of all modules loaded with image (executable)
                //  `modules_metadata` contains e.g. symbols of image modules loaded which had
                //                     PDB files present and we were able to load them.
                if (!found)
                {
                    sec_base = 0;

                    for (const auto& [key, value] : dll_metadata)
                    {
                        if (modules_metadata.count(key))
                        {
                            ModuleMetaData& mmd = modules_metadata[key];

                            for (auto& b : mmd.sym_info)
                                for (const auto& c : value.sec_info)
                                    if (c.idx == (b.sec_idx - 1))
                                    {
                                        sec_base = (UINT64)mmd.handle + c.offset;
                                        break;
                                    }

                            for (const auto& b : mmd.sym_info)
                                if (a.pc >= (b.offset + sec_base) && a.pc < (b.offset + sec_base + b.size))
                                {
                                    sd.desc = b;
                                    sd.desc.name = b.name + L":" + key;
                                    sd.module = &mmd;
                                    found = true;
                                    break;
                                }
                        }
                    }
                }

                if (!found)
                    sd.desc.name = L"unknown";

                for (uint32_t counter_idx = 0; counter_idx < 32; counter_idx++)
                {
                    if (!(a.ov_flags & (1i64 << (UINT64)counter_idx)))
                        continue;

                    bool inserted = false;
                    uint32_t event_src;
                    if (counter_idx == 31)
                        event_src = CYCLE_EVT_IDX;
                    else
                        event_src = request.ioctl_events_sample[counter_idx].index;
                    for (auto& c : resolved_samples)
                    {
                        if (c.desc.name == sd.desc.name && c.event_src == event_src)
                        {
                            c.freq++;
                            bool pc_found = false;
                            for (int i = 0; i < c.pc.size(); i++)
                            {
                                if (c.pc[i].first == a.pc)
                                {
                                    c.pc[i].second += 1;
                                    pc_found = true;
                                    break;
                                }
                            }

                            if (!pc_found)
                                c.pc.push_back(std::make_pair(a.pc, 1));

                            inserted = true;
                            break;
                        }
                    }

                    if (!inserted)
                    {
                        sd.freq = 1;
                        sd.event_src = event_src;
                        sd.pc.push_back(std::make_pair(a.pc, 1));
                        resolved_samples.push_back(sd);
                    }
                }
            }

            std::sort(resolved_samples.begin(), resolved_samples.end(), sort_samples);

            uint32_t prev_evt_src = 0;
            if (resolved_samples.size() > 0)
                prev_evt_src = resolved_samples[0].event_src;

            std::vector<uint64_t> total_samples;
            uint64_t acc = 0;
            for (const auto& a : resolved_samples)
            {
                if (a.event_src != prev_evt_src)
                {
                    prev_evt_src = a.event_src;
                    total_samples.push_back(acc);
                    acc = 0;
                }

                acc += a.freq;
            }
            total_samples.push_back(acc);

            int32_t group_idx = -1;
            prev_evt_src = CYCLE_EVT_IDX - 1;
            uint64_t printed_sample_num = 0, printed_sample_freq = 0;
            std::vector<std::wstring> col_symbol;
            std::vector<double> col_overhead;
            std::vector<uint32_t> col_count;

            std::vector<std::pair<GlobalStringType, TableOutput<SamplingAnnotateOutputTraitsL, GlobalCharType>>> annotateTables;
            for (auto &a : resolved_samples)
            {
                if (a.event_src != prev_evt_src)
                {
                    if (prev_evt_src != CYCLE_EVT_IDX - 1)
                    {
                        TableOutput<SamplingOutputTraitsL, GlobalCharType> table(m_outputType);
                        table.PresetHeaders();
                        table.SetAlignment(0, ColumnAlignL::RIGHT);
                        table.SetAlignment(1, ColumnAlignL::RIGHT);
                        table.Insert(col_overhead, col_count, col_symbol);
                        table.InsertExtra(L"interval", request.sampling_inverval[prev_evt_src]);
                        table.InsertExtra(L"printed_sample_num", printed_sample_num);
                        m_out.Print(table);
                        table.m_event = GlobalStringType(pmu_events::get_event_name(static_cast<uint16_t>(prev_evt_src)));
                        m_globalSamplingJSON.m_map[table.m_event] = std::make_pair(table, annotateTables);
                        col_overhead.clear();
                        col_count.clear();
                        col_symbol.clear();
                        annotateTables.clear();
                    }
                    prev_evt_src = a.event_src;

                    if (printed_sample_num > 0 && printed_sample_num < request.sample_display_row)
                        m_out.GetOutputStream()
                        << DoubleToWideStringExt(((double)printed_sample_freq * 100 / (double)total_samples[group_idx]), 2, 6) << L"%"
                        << IntToDecWideString(printed_sample_freq, 10)
                        << L"  top " << std::dec << printed_sample_num << L" in total" << std::endl;

                    m_out.GetOutputStream()
                        << L"======================== sample source: "
                        << pmu_events::get_event_name(static_cast<uint16_t>(a.event_src)) << L", top "
                        << std::dec << request.sample_display_row
                        << L" hot functions ========================" << std::endl;

                    printed_sample_num = 0;
                    printed_sample_freq = 0;
                    group_idx++;
                }

                if (printed_sample_num == request.sample_display_row)
                {
                    m_out.GetOutputStream() << DoubleToWideStringExt(((double)printed_sample_freq * 100 / (double)total_samples[group_idx]), 2, 6) << L"%"
                        << IntToDecWideString(printed_sample_freq, 10)
                        << L"  top " << std::dec << request.sample_display_row << L" in total" << std::endl;
                    printed_sample_num++;
                    continue;
                }

                if (printed_sample_num > request.sample_display_row)
                    continue;

                col_overhead.push_back(((double)a.freq * 100 / (double)total_samples[group_idx]));// +L"%");
                col_count.push_back(a.freq);
                col_symbol.push_back(a.desc.name);

                if (request.do_verbose)
                {
                    std::sort(a.pc.begin(), a.pc.end(), sort_pcs);

                    for (int i = 0; i < 10 && i < a.pc.size(); i++)
                    {
                        m_out.GetOutputStream() << L"                   " << IntToHexWideString(a.pc[i].first, 20) << L" " << IntToDecWideString(a.pc[i].second, 8) << std::endl;
                    }
                }

                if (request.do_export_perf_data)
                {
                    for (const auto& sample : a.pc)
                    {
                        ULONGLONG addr;
                        if (a.module == NULL)
                            addr = (sample.first - runtime_vaddr_delta) & 0xFFFFFF;
                        else
                        {
                            UINT64 mod_vaddr_delta = (UINT64)a.module->handle;
                            addr = (sample.first - mod_vaddr_delta) & 0xFFFFFF;
                        }
                        perfDataWriter.RegisterEvent(PerfDataWriter::SAMPLE, pid, sample.first, request.cores_idx[0]);
                    }
                }
                if (request.do_annotate)
                {
                    std::map<std::pair<std::wstring, DWORD>, uint64_t> hotspots;
                    std::vector<std::wstring> col_source_file;
                    std::vector<uint64_t> col_line_number, col_hits;
                    if(a.desc.name != L"unknown")
                    {
                        m_out.GetOutputStream() << a.desc.name << std::endl;
                        for (const auto& sample : a.pc)
                        {
                            bool found_line = false;
                            ULONGLONG addr;
                            if(a.module == NULL)
                                addr = (sample.first - runtime_vaddr_delta) & 0xFFFFFF;
                            else
                            {
                                UINT64 mod_vaddr_delta = (UINT64)a.module->handle;
                                addr = (sample.first - mod_vaddr_delta) & 0xFFFFFF;
                            }
                            for (const auto& line : a.desc.lines)
                            {
                                if (line.virtualAddress <= addr && line.virtualAddress + line.length > addr)
                                {
                                    std::pair<std::wstring, DWORD> cur = std::make_pair(line.source_file, line.lineNum);
                                    if (auto el = hotspots.find(cur); el == hotspots.end())
                                    {
                                        hotspots[cur] = sample.second;
                                    }
                                    else {
                                        hotspots[cur] += sample.second;
                                    }
                                    found_line = true;
                                }
                            }
                            if (!found_line)
                            {
                                m_out.GetErrorOutputStream() << "No line for " << std::hex << addr << " found." << std::endl;
                            }
                        }

                        std::vector<std::tuple<std::wstring, DWORD, uint64_t>>  sorting_annotate;
                        for (auto& [key, val] : hotspots)
                        {
                            sorting_annotate.push_back(std::make_tuple(key.first, key.second, val));
                        }
                        
                        std::sort(sorting_annotate.begin(), sorting_annotate.end(), [](std::tuple<std::wstring, DWORD, uint64_t>& a, std::tuple<std::wstring, DWORD, uint64_t>& b)->bool { return std::get<2>(a) > std::get<2>(b); });
                        
                        for (auto& el : sorting_annotate)
                        {
                            col_source_file.push_back(std::get<0>(el));
                            col_line_number.push_back(std::get<1>(el));
                            col_hits.push_back(std::get<2>(el));
                        }

                        if (col_source_file.size() > 0)
                        {
                            TableOutput<SamplingAnnotateOutputTraitsL, GlobalCharType> annotateTable;
                            annotateTable.PresetHeaders();
                            annotateTable.Insert(col_source_file, col_line_number, col_hits);
                            m_out.Print(annotateTable);
                            annotateTables.push_back(std::make_pair(a.desc.name, annotateTable));
                        }
                    }
                }

                printed_sample_freq += a.freq;
                printed_sample_num++;
            }
            
            if (request.do_export_perf_data)
                perfDataWriter.Write();

            TableOutput<SamplingOutputTraitsL, GlobalCharType> table(m_outputType);
            table.PresetHeaders();
            table.SetAlignment(0, ColumnAlignL::RIGHT);
            table.SetAlignment(1, ColumnAlignL::RIGHT);
            table.Insert(col_overhead, col_count, col_symbol);
            table.InsertExtra(L"interval", request.sampling_inverval[prev_evt_src]);
            table.InsertExtra(L"printed_sample_num", printed_sample_num);
            m_out.Print(table);
            table.m_event = GlobalStringType(pmu_events::get_event_name(static_cast<uint16_t>(prev_evt_src)));
            m_globalSamplingJSON.m_map[table.m_event] = std::make_pair(table, annotateTables);
            m_globalSamplingJSON.m_sample_display_row = request.sample_display_row;

            if (m_outputType == TableType::JSON || m_outputType == TableType::ALL)
            {
                m_out.Print(m_globalSamplingJSON);
            }

            if (printed_sample_num > 0 && printed_sample_num < request.sample_display_row)
                m_out.GetOutputStream() << DoubleToWideStringExt((double)printed_sample_freq * 100 / (double)total_samples[group_idx], 2, 6) << L"%"
                << IntToDecWideString(printed_sample_freq, 10) << L"  top " << std::dec << printed_sample_num << L" in total" << std::endl;
        }
    }
    catch (fatal_exception& e)
    {
        std::cerr << e.what() << std::endl;
        exit_code = EXIT_FAILURE;
        goto clean_exit;
    }

clean_exit:

    return exit_code;
}

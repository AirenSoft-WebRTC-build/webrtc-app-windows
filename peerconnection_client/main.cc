/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

 // clang-format off
 // clang formating would change include order.
#include <windows.h>
#include <shellapi.h>  // must come after windows.h
// clang-format on

#include <string>
#include <vector>

// #include "absl/flags/parse.h"
#include "conductor.h"
// #include "flag_defs.h"
#include "main_wnd.h"
#include "peer_connection_client.h"
#include "rtc_base/checks.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/string_utils.h"  // For ToUtf8
#include "rtc_base/win32_socket_init.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

namespace {
    // A helper class to translate Windows command line arguments into UTF8,
    // which then allows us to just pass them to the flags system.
    // This encapsulates all the work of getting the command line and translating
    // it to an array of 8-bit strings; all you have to do is create one of these,
    // and then call argc() and argv().
    class WindowsCommandLineArguments {
    public:
        WindowsCommandLineArguments();

        WindowsCommandLineArguments(const WindowsCommandLineArguments&) = delete;
        WindowsCommandLineArguments& operator=(WindowsCommandLineArguments&) = delete;

        int argc() { return argv_.size(); }
        char** argv() { return argv_.data(); }

    private:
        // Owned argument strings.
        std::vector<std::string> args_;
        // Pointers, to get layout compatible with char** argv.
        std::vector<char*> argv_;
    };

    WindowsCommandLineArguments::WindowsCommandLineArguments() {
        // start by getting the command line.
        LPCWSTR command_line = ::GetCommandLineW();
        // now, convert it to a list of wide char strings.
        int argc;
        LPWSTR* wide_argv = ::CommandLineToArgvW(command_line, &argc);

        // iterate over the returned wide strings;
        for (int i = 0; i < argc; ++i) {
            args_.push_back(rtc::ToUtf8(wide_argv[i], wcslen(wide_argv[i])));
            // make sure the argv array points to the string data.
            argv_.push_back(const_cast<char*>(args_.back().c_str()));
        }
        LocalFree(wide_argv);
    }
	
    class CustomSocketServer : public rtc::PhysicalSocketServer {
    public:
        bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override {
            if (!process_io)
            {
                return true;
            }
            return rtc::PhysicalSocketServer::Wait(webrtc::TimeDelta::Zero(), process_io);
        }
    };

}  // namespace

int PASCAL wWinMain(HINSTANCE instance,
    HINSTANCE prev_instance,
    wchar_t* cmd_line,
    int cmd_show) 
{
    rtc::WinsockInitializer winsock_init;
    CustomSocketServer ss;
    rtc::AutoSocketServerThread main_thread(&ss);

    WindowsCommandLineArguments win_args;
    int argc = win_args.argc();
    char** argv = win_args.argv();

    const std::string server = "127.0.0.1";
    MainWnd wnd(server.c_str(), 8888, false, false);
    if (!wnd.Create()) {
        RTC_DCHECK_NOTREACHED();
        return -1;
    }

    rtc::InitializeSSL();
    PeerConnectionClient client;
    auto conductor = rtc::make_ref_counted<Conductor>(&client, &wnd);

    main_thread.Start();

    // Main loop.
    MSG msg;
    BOOL gm;
    while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
        if (!wnd.PreTranslateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    if (conductor->connection_active() || client.is_connected()) {
        while ((conductor->connection_active() || client.is_connected()) &&
            (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
            if (!wnd.PreTranslateMessage(&msg)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }

    rtc::CleanupSSL();
	
    conductor->Close();
    main_thread.Stop();

    return 0;
}

/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018-2026 Folke Will and Avitab Contributors
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "App.h"
#include "gui/widgets/Container.h"
#include "gui/widgets/PixMap.h"
#include "gui/widgets/Label.h"
#include "gui/widgets/TextArea.h"
#include "gui/widgets/Keyboard.h"
#include "image/Image.h"

namespace avitab {

/**
 * LATAM ACARS: shows one page of the LatamAcars EFB, rendered by the LatamAcars desktop app, as a stream of JPEG frames
 * (GET /frame) and sends touches and the mouse wheel back (POST /api/input). It has no UI logic of its own.
 * Each page (dashboard, brief, radar, messages) is a separate app on the launcher: see the subclasses below.
 */
class LatamApp: public App {
public:
    // page: the EFB page to open ("flight", "brief", "weather", "docs", "mras", "radar" or "messages"); title: the name of the app
    LatamApp(FuncsPtr appFuncs, const std::string &page, const std::string &title);
    void resume() override;
    void suspend() override;
    void onMouseWheel(int dir, int x, int y) override;
    ~LatamApp();

private:
    struct InputEvent {
        std::string type;       // down, move, up, wheel, nav, text, key, blur
        int x = 0, y = 0, dy = 0;
        std::string text;       // the typed text, or the name of the key
    };

    const std::string page;

    // No window frame (no title bar, no close button): the page fills the whole app area. The AviTab home button leaves the app.
    std::shared_ptr<Container> host;
    std::shared_ptr<PixMap> screen;
    std::shared_ptr<Label> status;
    img::Image image;

    // Deferred UI callbacks check this before touching the app (it may be gone by then).
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    int width = 0, height = 0;

    // On-screen keyboard: shown while a text field of the page has the focus. The page is then asked for a shorter
    // frame so that the field is not hidden behind the keyboard.
    std::shared_ptr<TextArea> kbdTarget;    // the keyboard needs a target; typing keys do not edit it
    std::shared_ptr<Keyboard> keys;
    int kbdHeight = 0;
    bool kbdVisible = false;
    bool kbdSuppressed = false;             // the user closed the keyboard: stays closed until the field loses focus
    std::atomic<int> frameHeight{0};

    std::atomic<bool> running{false};
    bool cancelFrame = false, cancelInput = false;
    std::thread frameThread, inputThread;

    std::mutex inputMutex;
    std::condition_variable inputCv;
    std::deque<InputEvent> inputQueue;

    std::mutex urlMutex;
    std::string baseUrl;

    std::string getBaseUrl();
    void reloadBaseUrl();
    void setStatus(const std::string &text);
    void enqueue(const InputEvent &ev);
    void requestPage();
    void onKeyboardWanted(bool wanted);     // UI thread
    void showKeyboard();
    void hideKeyboard(bool byUser);
    void frameLoop();
    void inputLoop();
    void stopThreads();
};

// One launcher icon per EFB page.
class LatamDashboardApp: public LatamApp {
public:
    LatamDashboardApp(FuncsPtr appFuncs): LatamApp(appFuncs, "flight", "FLIGHT") {}
};

class LatamBriefApp: public LatamApp {
public:
    LatamBriefApp(FuncsPtr appFuncs): LatamApp(appFuncs, "brief", "BRIEF") {}
};

class LatamWeatherApp: public LatamApp {
public:
    LatamWeatherApp(FuncsPtr appFuncs): LatamApp(appFuncs, "weather", "WEATHER") {}
};

class LatamDocsApp: public LatamApp {
public:
    LatamDocsApp(FuncsPtr appFuncs): LatamApp(appFuncs, "docs", "DOCS") {}
};

class LatamMrasApp: public LatamApp {
public:
    LatamMrasApp(FuncsPtr appFuncs): LatamApp(appFuncs, "mras", "MRAS") {}
};

class LatamRadarApp: public LatamApp {
public:
    LatamRadarApp(FuncsPtr appFuncs): LatamApp(appFuncs, "radar", "RADAR") {}
};

class LatamMessagesApp: public LatamApp {
public:
    LatamMessagesApp(FuncsPtr appFuncs): LatamApp(appFuncs, "messages", "MESSAGES") {}
};

class LatamLogbookApp: public LatamApp {
public:
    LatamLogbookApp(FuncsPtr appFuncs): LatamApp(appFuncs, "logbook", "LOGBOOK") {}
};

class LatamAisApp: public LatamApp {
public:
    LatamAisApp(FuncsPtr appFuncs): LatamApp(appFuncs, "ais", "AIS") {}
};

class LatamOptApp: public LatamApp {
public:
    LatamOptApp(FuncsPtr appFuncs): LatamApp(appFuncs, "opt", "OPTIMIZATION") {}
};

class LatamSettingsApp: public LatamApp {
public:
    LatamSettingsApp(FuncsPtr appFuncs): LatamApp(appFuncs, "settings", "SETTINGS") {}
};

// Navigraph Charts (https://charts.navigraph.com/): LatamAcars opens the site in its off-screen browser instead of an EFB page.
class LatamNavigraphApp: public LatamApp {
public:
    LatamNavigraphApp(FuncsPtr appFuncs): LatamApp(appFuncs, "navigraph", "NAVIGRAPH") {}
};

} /* namespace avitab */

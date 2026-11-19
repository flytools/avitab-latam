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
#include "LatamApp.h"
#include "Logger.h"
#include "charts/RESTClient.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>

namespace avitab {

namespace {
const char *DEFAULT_URL = "http://127.0.0.1:47615";

void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
}

LatamApp::LatamApp(FuncsPtr appFuncs, const std::string &pageName, const std::string &title):
    App(appFuncs),
    page(pageName),
    host(std::make_shared<Container>(getUIContainer()))
{
    (void) title;       // no title bar: the app is named by its icon on the home screen

    host->setPosition(0, 0);
    host->setDimensions(getUIContainer()->getWidth(), getUIContainer()->getHeight());
    width = host->getWidth();
    height = host->getHeight();
    image.resize(width, height, img::COLOR_BLACK);

    screen = std::make_shared<PixMap>(host);
    screen->draw(image);
    screen->setClickable(true);
    screen->setClickHandler([this] (int x, int y, bool start, bool stop) {
        InputEvent ev;
        ev.type = start ? "down" : (stop ? "up" : "move");
        ev.x = x;
        ev.y = y;
        enqueue(ev);
    });

    status = std::make_shared<Label>(host, "Connecting to LatamAcars...");
    status->centerInParent();

    // The on-screen keyboard (hidden until a text field of the page gets the focus). Its keys are sent to the page.
    frameHeight = height;
    kbdHeight = std::max(180, height * 4 / 10);
    kbdTarget = std::make_shared<TextArea>(host, "");
    kbdTarget->setVisible(false);
    keys = std::make_shared<Keyboard>(host, kbdTarget);
    keys->setDimensions(width, kbdHeight);
    keys->setPosition(0, height - kbdHeight);
    keys->setVisible(false);
    keys->setOnKey([this] (const std::string &key, bool special) {
        InputEvent ev;
        ev.type = special ? "key" : "text";
        ev.text = key;
        enqueue(ev);
    });
    keys->setOnOk([this] () { hideKeyboard(true); });
    keys->setOnCancel([this] () { hideKeyboard(true); });

    baseUrl = DEFAULT_URL;
}

LatamApp::~LatamApp() {
    *alive = false;
    stopThreads();
}

// ---------- lifecycle: only talk to LatamAcars while the app is on screen ----------

void LatamApp::resume() {
    App::resume();
    if (running) {
        return;
    }
    reloadBaseUrl();
    running = true;
    requestPage();
    frameThread = std::thread([this] () { frameLoop(); });
    inputThread = std::thread([this] () { inputLoop(); });
}

// Asks LatamAcars to show this app's page (the page itself has no menu here: the launcher is the menu).
void LatamApp::requestPage() {
    InputEvent ev;
    ev.type = "nav";
    enqueue(ev);
}

void LatamApp::suspend() {
    stopThreads();
    if (kbdVisible) {
        hideKeyboard(false);
    }
    kbdSuppressed = false;
    App::suspend();
}

// ---------- on-screen keyboard ----------

// The page tells, with every frame, whether a text field has the focus.
void LatamApp::onKeyboardWanted(bool wanted) {
    if (wanted) {
        if (!kbdVisible && !kbdSuppressed) {
            showKeyboard();
        }
    } else {
        kbdSuppressed = false;      // the field lost the focus: the next field may open the keyboard again
        if (kbdVisible) {
            hideKeyboard(false);
        }
    }
}

void LatamApp::showKeyboard() {
    kbdVisible = true;
    frameHeight = height - kbdHeight;       // the page gets shorter, so the field stays above the keyboard
    keys->setVisible(true);
}

void LatamApp::hideKeyboard(bool byUser) {
    kbdVisible = false;
    frameHeight = height;
    keys->setVisible(false);
    if (byUser) {
        kbdSuppressed = true;
        InputEvent ev;
        ev.type = "blur";       // take the focus out of the field, or the page would ask for the keyboard again
        enqueue(ev);
    }
}

void LatamApp::stopThreads() {
    running = false;
    cancelFrame = true;     // aborts a request in flight (RESTClient polls this flag)
    cancelInput = true;
    inputCv.notify_all();
    if (frameThread.joinable()) {
        frameThread.join();
    }
    if (inputThread.joinable()) {
        inputThread.join();
    }
    std::lock_guard<std::mutex> lock(inputMutex);
    inputQueue.clear();
}

// ---------- where LatamAcars listens ----------

// LatamAcars writes its port to %LOCALAPPDATA%\LatamAcars\efb.json (it picks the next one if the default is busy).
void LatamApp::reloadBaseUrl() {
    std::string url = DEFAULT_URL;
    try {
        const char *local = std::getenv("LOCALAPPDATA");
        if (local) {
            std::ifstream in(std::string(local) + "/LatamAcars/efb.json");
            if (in) {
                auto json = nlohmann::json::parse(in);
                url = "http://127.0.0.1:" + std::to_string(json.at("port").get<int>());
            }
        }
    } catch (const std::exception &e) {
        logger::warn("LatamApp: cannot read efb.json: %s", e.what());
    }

    std::lock_guard<std::mutex> lock(urlMutex);
    baseUrl = url;
}

std::string LatamApp::getBaseUrl() {
    std::lock_guard<std::mutex> lock(urlMutex);
    return baseUrl;
}

void LatamApp::setStatus(const std::string &text) {
    auto guard = alive;
    api().executeLater([this, guard, text] () {
        if (!*guard) {
            return;
        }
        status->setText(text);
        status->setVisible(!text.empty());
    });
}

// ---------- frames: LatamAcars -> tablet ----------

void LatamApp::frameLoop() {
    apis::RESTClient client;
    client.setVerbose(false);
    std::string etag;
    bool connected = false;
    bool keyboardWanted = false;

    while (running) {
        try {
            std::string url = getBaseUrl() + "/frame?w=" + std::to_string(width) + "&h=" + std::to_string(frameHeight.load()) + "&since=" + etag;
            auto data = client.getBinary(url, cancelFrame);

            if (!connected) {
                connected = true;
                setStatus("");
                requestPage();      // LatamAcars may have been started after this app was opened
            }

            // What the page tells travels as parameters of the Content-Type: "image/jpeg; etag=<id>; kbd=<0|1>"
            // (etag: the frame id; kbd: a text field has the focus, so the keyboard should be shown)
            std::string type = client.getContentType();
            bool wantKeyboard = type.find("kbd=1") != std::string::npos;
            if (wantKeyboard != keyboardWanted) {
                keyboardWanted = wantKeyboard;
                auto kguard = alive;
                api().executeLater([this, kguard, wantKeyboard] () {
                    if (*kguard) {
                        onKeyboardWanted(wantKeyboard);
                    }
                });
            }

            if (data.empty()) {
                sleepMs(60);        // the page did not change
                continue;
            }

            auto pos = type.find("etag=");
            if (pos != std::string::npos) {
                auto end = type.find(';', pos);
                etag = type.substr(pos + 5, end == std::string::npos ? std::string::npos : end - (pos + 5));
            } else {
                etag = "";
            }

            auto guard = alive;
            api().executeLater([this, guard, data] () {
                if (!*guard) {
                    return;
                }
                try {
                    image.loadEncodedData(data, false);
                    screen->draw(image);
                } catch (const std::exception &e) {
                    logger::warn("LatamApp: bad frame: %s", e.what());
                }
            });
            sleepMs(40);            // at most ~20 frames per second
        } catch (const apis::HTTPException &e) {
            // 503: the LatamAcars window is still opening
            setStatus("LatamAcars is starting...");
            connected = false;
            sleepMs(700);
        } catch (const std::exception &e) {
            if (!running) {
                break;
            }
            setStatus("Waiting for LatamAcars...\nOpen the LatamAcars app on the PC.");
            connected = false;
            reloadBaseUrl();
            sleepMs(1500);
        }
    }
}

// ---------- input: tablet -> LatamAcars ----------

void LatamApp::enqueue(const InputEvent &ev) {
    if (!running) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        // a drag produces many moves: only the latest one matters
        if (ev.type == "move" && !inputQueue.empty() && inputQueue.back().type == "move") {
            inputQueue.back() = ev;
        } else {
            inputQueue.push_back(ev);
        }
    }
    inputCv.notify_one();
}

void LatamApp::onMouseWheel(int dir, int x, int y) {
    if (!screen->screenToLocal(x, y)) {
        return;
    }
    InputEvent ev;
    ev.type = "wheel";
    ev.x = x;
    ev.y = y;
    ev.dy = dir > 0 ? -120 : 120;   // browser convention: negative deltaY = scroll up / zoom in
    enqueue(ev);
}

void LatamApp::inputLoop() {
    apis::RESTClient client;
    client.setVerbose(false);

    while (running) {
        InputEvent ev;
        {
            std::unique_lock<std::mutex> lock(inputMutex);
            inputCv.wait(lock, [this] () { return !running || !inputQueue.empty(); });
            if (!running) {
                break;
            }
            ev = inputQueue.front();
            inputQueue.pop_front();
        }

        try {
            if (ev.type == "nav") {
                client.post(getBaseUrl() + "/api/nav", {
                    {"page", page},
                    {"nav", "0"},
                    {"w", std::to_string(width)},
                    {"h", std::to_string(height)},
                }, cancelInput);
                continue;
            }
            client.post(getBaseUrl() + "/api/input", {
                {"type", ev.type},
                {"x", std::to_string(ev.x)},
                {"y", std::to_string(ev.y)},
                {"dy", std::to_string(ev.dy)},
                {"text", ev.text},      // type=text: the typed text
                {"key", ev.text},       // type=key: Backspace, Enter, ArrowLeft, ArrowRight
            }, cancelInput);
        } catch (const std::exception &e) {
            if (running) {
                sleepMs(200);   // LatamAcars is not reachable; the frame loop shows the message
            }
        }
    }
}

} /* namespace avitab */

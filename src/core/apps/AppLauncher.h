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

#include <vector>
#include <memory>
#include <functional>
#include "App.h"
#include "gui/widgets/Button.h"
#include "gui/widgets/Label.h"
#include "gui/widgets/PixMap.h"
#include "gui/widgets/Container.h"
#include "image/Image.h"

namespace avitab {

class AppLauncher: public App {
public:
    using Callback = std::function<void()>;
    struct Entry {
        AppId id;
        std::shared_ptr<App> app;
        std::shared_ptr<Button> button;
    };

    AppLauncher(FuncsPtr appFuncs);
    void onScreenResize(int width, int height) override;
    void onPlaneLoad() override;
    void onMouseWheel(int dir, int x, int y) override;
    void changeChartTab(bool next) override;
    void recentre() override;
    void pan(int x, int y) override;
    void show() override;
    void showApp(AppId id);
private:
    std::vector<Entry> entries;

    std::shared_ptr<App> activeApp;

    // The home screen background (a picture behind the icons), fitted to the screen.
    std::shared_ptr<PixMap> background;
    img::Image backgroundImage;
    int bgWidth = 0, bgHeight = 0;
    void updateBackground(int width, int height);

    // The icons are placed by hand: 5 per row, each row centred, side margins, and a line between the LatamAcars apps and the others.
    std::shared_ptr<Container> divider;
    int layoutWidth = 0;
    void updateLayout(int width);

    template<typename T>
    void addEntry(const std::string &name, const std::filesystem::path &icon, AppId id);
};

} /* namespace avitab */

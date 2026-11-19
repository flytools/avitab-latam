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
#include "Logger.h"
#include "AppLauncher.h"
#include "ChartsApp.h"
#include "NotesApp.h"
#include "LatamApp.h"
#include "About.h"
#include "PlaneManualApp.h"
#include "AirportApp.h"
#include "RouteApp.h"
#include "MapApp.h"
#include "ProvidersApp.h"
#include "image/Image.h"
#include <algorithm>
#include <cmath>

namespace avitab {

AppLauncher::AppLauncher(FuncsPtr appFuncs):
    App(appFuncs)
{
    auto cont = getUIContainer();
    auto root = api().getAvitabInstallDir()/"icons";

    // the LatamAcars apps come first
    addEntry<LatamDashboardApp>("Flight", root / "latam_dashboard.png", AppId::LATAM_DASHBOARD);
    addEntry<LatamBriefApp>("Brief", root / "latam_brief.png", AppId::LATAM_BRIEF);
    addEntry<LatamWeatherApp>("Weather", root / "latam_weather.png", AppId::LATAM_WEATHER);
    addEntry<LatamDocsApp>("Docs", root / "latam_docs.png", AppId::LATAM_DOCS);
    addEntry<LatamMrasApp>("MRAs", root / "latam_mras.png", AppId::LATAM_MRAS);
    addEntry<LatamRadarApp>("Radar", root / "latam_radar.png", AppId::LATAM_RADAR);
    addEntry<LatamMessagesApp>("Messages", root / "latam_messages.png", AppId::LATAM_MESSAGES);

    addEntry<ChartsApp>("Charts", root / "folder.png", AppId::CHARTS);
    addEntry<AirportApp>("Airports", root / "if_xmag_3617.png", AppId::AIRPORTS);
    addEntry<RouteApp>("Routes", root / "if_applications-internet_118835.png", AppId::ROUTES);
    addEntry<MapApp>("Maps", root / "if_starthere_18227.png", AppId::MAPS);
    addEntry<PlaneManualApp>("Aircraft", root / "if_ilustracoes_04-11_1519786.png", AppId::MANUALS);
    addEntry<NotesApp>("Notes", root / "if_txt2_3783.png", AppId::NOTES);

    if (api().getChartService()->getNavigraph()->isSupported() || api().getChartService()->getChartFox()->isSupported()) {
        addEntry<ProvidersApp>("Providers", root / "if_Airport_22906.png", AppId::PROVIDERS);
    }

    addEntry<About>("About", root / "if_Help_1493288.png", AppId::ABOUT);

    updateLayout(cont->getWidth());
    updateBackground(cont->getWidth(), cont->getHeight());
}

namespace {
bool isLatamApp(AppId id) {
    switch (id) {
    case AppId::LATAM_DASHBOARD:
    case AppId::LATAM_BRIEF:
    case AppId::LATAM_WEATHER:
    case AppId::LATAM_DOCS:
    case AppId::LATAM_MRAS:
    case AppId::LATAM_RADAR:
    case AppId::LATAM_MESSAGES:
        return true;
    default:
        return false;
    }
}
}

// Five icons per row with room on both sides; a row with fewer icons is centred as a whole.
// The LatamAcars apps come first, then a thin line, then the AviTab apps.
void AppLauncher::updateLayout(int width) {
    if (width <= 0 || width == layoutWidth) {
        return;
    }
    layoutWidth = width;

    const int columns = 5;
    const int tileWidth = 90;
    const int top = 18, rowGap = 14, groupGap = 18;

    int margin = std::max(40, width / 8);
    int usable = width - 2 * margin;
    if (usable < columns * tileWidth) {
        usable = std::min(width, columns * tileWidth);
        margin = (width - usable) / 2;
    }
    const int cellWidth = usable / columns;

    int y = top;
    auto placeGroup = [&] (bool latam) {
        std::vector<Entry *> group;
        for (auto &entry: entries) {
            if (isLatamApp(entry.id) == latam) {
                group.push_back(&entry);
            }
        }
        for (size_t first = 0; first < group.size(); first += columns) {
            size_t count = std::min<size_t>(columns, group.size() - first);
            int x0 = (width - (int) count * cellWidth) / 2;
            int rowHeight = 0;
            for (size_t i = 0; i < count; i++) {
                auto &button = group[first + i]->button;
                button->setPosition(x0 + (int) i * cellWidth + (cellWidth - button->getWidth()) / 2, y);
                rowHeight = std::max(rowHeight, button->getHeight());
            }
            y += rowHeight + rowGap;
        }
    };

    placeGroup(true);

    // the line between the two groups
    if (!divider) {
        divider = std::make_shared<Container>(getUIContainer());
        static lv_style_t dividerStyle;
        lv_style_copy(&dividerStyle, &lv_style_plain);
        dividerStyle.body.main_color = lv_color_hex(0xFFFFFF);
        dividerStyle.body.grad_color = lv_color_hex(0xFFFFFF);
        dividerStyle.body.opa = LV_OPA_30;
        dividerStyle.body.radius = LV_RADIUS_CIRCLE;
        dividerStyle.body.border.width = 0;
        dividerStyle.body.shadow.width = 0;
        lv_obj_set_style(divider->obj(), &dividerStyle);
    }
    y += 4;
    divider->setDimensions(usable, 2);
    divider->setPosition(margin, y);
    y += 2 + groupGap;

    placeGroup(false);
}

// The home screen background: icons/latam_bg_<width>.jpg (the LatamAcars picture, already darkened and blurred), in a few sizes.
// The smallest one that is at least as wide as the screen is scaled to cover it and cropped around the centre.
// Without the files the home screen stays as it was.
void AppLauncher::updateBackground(int width, int height) {
    if (width <= 0 || height <= 0 || (width == bgWidth && height == bgHeight)) {
        return;
    }
    bgWidth = width;
    bgHeight = height;

    try {
        static const int sizes[] = {1024, 1536, 2048};
        int pick = sizes[2];
        for (int size: sizes) {
            if (size >= width) {
                pick = size;
                break;
            }
        }

        img::Image source;
        source.loadImageFile(api().getAvitabInstallDir() / "icons" / ("latam_bg_" + std::to_string(pick) + ".jpg"));

        double k = std::max(width / (double) source.getWidth(), height / (double) source.getHeight());
        int scaledW = (int) std::ceil(source.getWidth() * k);
        int scaledH = (int) std::ceil(source.getHeight() * k);
        if (scaledW != source.getWidth() || scaledH != source.getHeight()) {
            source.scale(scaledW, scaledH);
        }

        backgroundImage.resize(width, height, img::COLOR_BLACK);
        source.copyTo(backgroundImage, (scaledW - width) / 2, (scaledH - height) / 2);

        if (!background) {
            background = std::make_shared<PixMap>(getUIContainer());
            background->sendToBackground();
        }
        background->draw(backgroundImage);
    } catch (const std::exception &e) {
        logger::warn("Home screen background not available: %s", e.what());
    }
}

void AppLauncher::onScreenResize(int width, int height) {
    updateLayout(width);
    updateBackground(width, height);
    for (auto &entry: entries) {
        entry.app->onScreenResize(width, height);
    }
}

void AppLauncher::show() {
    if (activeApp) {
        activeApp->suspend();
    }
    App::show();
    api().notifyHomeScreenActive(true);
}

void AppLauncher::showApp(AppId id) {
    for (auto &entry: entries) {
        if (entry.id == id) {
            if (activeApp) {
                activeApp->suspend();
            }
            activeApp = entry.app;
            activeApp->resume();
            activeApp->show();
            api().notifyHomeScreenActive(false);
            break;
        }
    }
}

template<typename T>
void AppLauncher::addEntry(const std::string& name, const std::filesystem::path& icon, AppId id) {
    auto app = startSubApp<T>();
    app->setOnExit([this] () {
        this->show();
    });

    img::Image iconImg;
    try {
        iconImg.loadImageFile(icon);
        iconImg.scale(48, 48);
    } catch (const std::exception &e) {
        logger::warn("Couldn't load icon %s: %s", icon.string().c_str(), e.what());
    }

    Entry entry;
    entry.id = id;
    entry.app = std::move(app);
    entry.button = std::make_shared<Button>(getUIContainer(), std::move(iconImg), name, 90);
    entries.push_back(entry);

    size_t index = entries.size() - 1;
    entry.button->setCallback([this, index] (const Button &) {
        showApp(entries[index].id);
    });
}

void AppLauncher::onPlaneLoad() {
    for (auto &entry: entries) {
        entry.app->onPlaneLoad();
    }
}

void AppLauncher::onMouseWheel(int dir, int x, int y) {
    if (activeApp) {
        activeApp->onMouseWheel(dir, x, y);
    }
}

void AppLauncher::changeChartTab(bool next) {
    if (activeApp) {
        activeApp->changeChartTab(next);
    }
}

void AppLauncher::recentre() {
    if (activeApp) {
        activeApp->recentre();
    }
}

void AppLauncher::pan(int x, int y) {
    if (activeApp) {
        activeApp->pan(x, y);
    }
}


} /* namespace avitab */

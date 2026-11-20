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

#include <functional>
#include "TextArea.h"
#include "Widget.h"

namespace avitab {

class Keyboard: public Widget {
public:
    using Callback = std::function<void()>;
    // Called for every typing key instead of editing the target text area. "special" keys are named
    // "Backspace", "Enter", "ArrowLeft" and "ArrowRight"; any other key is the text of the key itself.
    using KeyCallback = std::function<void(const std::string &key, bool special)>;

    Keyboard(WidgetPtr parent, std::shared_ptr<TextArea> target);
    void setOnKey(KeyCallback cb);
    // A key of the physical keyboard ('\b' = backspace, '\n' = enter, otherwise a Unicode code point).
    // Returns true when it was passed to the key callback (so the target text area must be left alone).
    bool forwardPhysicalKey(uint32_t c);
    void setTarget(std::shared_ptr<TextArea> target);
    void setOnCancel(Callback cb);
    void setOnOk(Callback cb);
    void hideEnterKey();
    void setNumericLayout();

    bool hasOkAction() const;
private:
    std::shared_ptr<TextArea> targetText;
    Callback onCancel;
    Callback onOk;
    KeyCallback onKey;

    bool forwardKey(lv_obj_t *keys);
};

} /* namespace avitab */

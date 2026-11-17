#ifndef __WIRE_TOOLS_MENU_H__
#define __WIRE_TOOLS_MENU_H__
#if !defined(LITE_VERSION)
#include <MenuItemInterface.h>

class WireToolsMenu : public MenuItemInterface {
public:
    WireToolsMenu() : MenuItemInterface("Wire Tools") {}

    void optionsMenu(void);
    void drawIcon(float scale);
#if defined(UI_COMPACT) && UI_COMPACT
    CompactMenuIcon compactIcon() const override { return CompactMenuIcon::Diagnostics; }
#endif

    bool hasTheme() { return bruceConfig.theme.wire; }
    const String& themePath() override { return bruceConfig.theme.paths.wire; }
};

#endif
#endif

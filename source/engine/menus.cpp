#include "engine.h"

void notifywelcome()
{
    UI::hideui("servers");
}

struct change
{
    int type;
    const char *desc;

    change() {}
    change(int type, const char *desc) : type(type), desc(desc) {}
};
static vector<change> needsapply;

VARP(applydialog, 0, 1, 1);
VAR(hidechanges, 0, 0, 1);

void addchange(const char *desc, int type)
{
    if(!applydialog) return;
    loopv(needsapply) if(!strcmp(needsapply[i].desc, desc)) return;
    needsapply.add(change(type, desc));
    if(!hidechanges) UI::showui("changes");
}

void clearchanges(int type)
{
    loopvrev(needsapply)
    {
        change &c = needsapply[i];
        if(c.type&type)
        {
            c.type &= ~type;
            if(!c.type) needsapply.remove(i);
        }
    }
    if(needsapply.empty()) UI::hideui("changes");
}

void applychanges()
{
    int changetypes = 0;
    loopv(needsapply) changetypes |= needsapply[i].type;
    if(changetypes&CHANGE_GFX) execident("resetgl");
    else if(changetypes&CHANGE_SHADERS) execident("resetshaders");
    if(changetypes&CHANGE_SOUND) execident("resetsound");
}

COMMAND(applychanges, "");
ICOMMAND(pendingchanges, "b", (int *idx), { if(needsapply.inrange(*idx)) result(needsapply[*idx].desc); else if(*idx < 0) intret(needsapply.length()); });

static int lastmainmenu = -1;

void menuprocess()
{
    if(lastmainmenu != mainmenu)
    {
        lastmainmenu = mainmenu;
        execident("on_mainmenutoggle");
    }

    // specific UIs
    if(!UI::uivisible("permanent"))
    {
        UI::showui("permanent"); // always force the permanent UI elements
    }
    if(mainmenu)
    {
        // force the main menu UIs
        if(mainmenu == MENU_MAP) UI::showui("main");
        else UI::showui("main_welcome");
    }
    if(isconnected(true))
    {
        if(mainmenu == MENU_MAP) UI::showui("main");
        else if(!UI::uivisible("gamehud"))
        {
            UI::showui("gamehud");  // if connected, force the game HUD
        }
    }
}

VAR(mainmenu, 2, 1, 0);
SVAR(mainmenumap, "main");

void loadmainmenu()
{
    if(isconnected()) disconnect();
    if(mainmenu != MENU_MAP)
    {
        mainmenu = MENU_MAP;
        game::loadMainMenuMap(mainmenumap);
        if(UI::uivisible("main_welcome"))
        {
            UI::hideui("main_welcome");
        }
    }
    else
    {
        mainmenu = MENU_INTRO;
        if(UI::uivisible("main"))
        {
            UI::hideui("main");
        }
        playintromusic();
    }
}
COMMAND(loadmainmenu, "");

void clearmainmenu(const char* mapname)
{
    hidechanges = 0;
    if(mainmenu && isconnected())
    {
        if(!mapname || game::editing() || strcmp(mapname, mainmenumap))
        {
            mainmenu = 0;
        }
        else
        {
            mainmenu = 2;
        }
        UI::hideui(NULL);
    }
}


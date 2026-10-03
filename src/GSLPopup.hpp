#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

// Окно со всем листом: вкладки по спискам, клик по уровню открывает его.
class GSLPopup : public Popup {
public:
    static GSLPopup* create();

protected:
    bool init();
    void selectList(std::string const& list);
    void onTab(CCObject* sender);
    void onLevel(CCObject* sender);
    void onRefresh(CCObject*);

    ScrollLayer* m_scroll = nullptr;
    CCMenu* m_tabs = nullptr;
    CCLabelBMFont* m_status = nullptr;
    std::string m_current = "classic";
};

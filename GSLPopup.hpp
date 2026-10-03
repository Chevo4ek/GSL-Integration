#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include "GSLManager.hpp"

using namespace geode::prelude;

// Экран листа в стиле Demonlist: настоящие карточки уровней,
// вкладки списков, поиск по названию и страницы по 10 уровней.
class GSLListLayer : public CCLayer, public LevelManagerDelegate {
public:
    static CCScene* scene();
    static GSLListLayer* create();

    void loadLevelsFinished(CCArray* levels, char const* key, int type);
    void loadLevelsFinished(CCArray* levels, char const* key);
    void loadLevelsFailed(char const* key, int type);
    void loadLevelsFailed(char const* key);

protected:
    bool init() override;
    void keyBackClicked() override;
    void onExit() override;

    void onBack(CCObject*);
    void onTab(CCObject* sender);
    void onPrev(CCObject*);
    void onNext(CCObject*);
    void onSearch(CCObject*);
    void onRefresh(CCObject*);
    void onSite(CCObject*);
    void onFilter(CCObject*);

    void selectList(std::string const& list);
    void loadPage();
    void showLevels(CCArray* levels);
    std::vector<GSLEntry> filtered() const;

    std::string m_list = "classic";
    std::string m_query;
    std::vector<GSLEntry> m_entries;
    std::vector<int> m_pageIds;
    int m_page = 0;
    std::string m_key;

    GJListLayer* m_listLayer = nullptr;
    TextInput* m_search = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCNode* m_note = nullptr;
    CCScale9Sprite* m_filterBg = nullptr;
    bool m_hideCompleted = false;
    CCMenuItemSpriteExtra* m_prev = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
    std::vector<ButtonSprite*> m_tabSprites;
};

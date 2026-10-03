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
    void onPrev(CCObject*);
    void onNext(CCObject*);
    void onSearch(CCObject*);
    void onRefresh(CCObject*);
    void onSite(CCObject*);
    void onFilter(CCObject*);
    void onPickList(CCObject*);
    void onGoToPage(CCObject*);
    void onRandomPage(CCObject*);
    void onLastPage(CCObject*);

public:
    // фильтры (окно фильтров меняет их и вызывает applyFilters)
    int m_rankLimit = 0;          // 0 = все, иначе Top N
    bool m_hideCompleted = false;
    bool m_onlyCreator = false;
    std::string m_creator;
    void applyFilters();
    void selectListPublic(std::string const& list) { selectList(list); }
    void goToPage(int page);
    int pageCount() const;

protected:

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
    ButtonSprite* m_listBtnSpr = nullptr;
    CCLabelBMFont* m_pageBtnLabel = nullptr;
    CCMenuItemSpriteExtra* m_prev = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
};

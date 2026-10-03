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

    void loadLevelsFinished(CCArray* levels, char const* key, int type) override;
    void loadLevelsFinished(CCArray* levels, char const* key) override;
    void loadLevelsFailed(char const* key, int type) override;
    void loadLevelsFailed(char const* key) override;

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
    void onInfo(CCObject*);
    void onFilter(CCObject*);
    void onPickList(CCObject*);
    void onGoToPage(CCObject*);
    void onRandomPage(CCObject*);
    void onLastPage(CCObject*);

public:
    // фильтры (окно фильтров меняет их и вызывает applyFilters)
    int m_rankLimit = 0;          // 0 = все, иначе Top N
    int m_lengthMask = 0;         // биты: 0 Tiny, 1 Short, 2 Medium, 3 Long, 4 XL, 5 Platformer; 0 = любая
    bool m_completedBy = false;
    std::string m_victor;
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
    std::vector<GSLEntry> filtered(bool useGD = true) const;
    bool needsGDInfo() const { return m_lengthMask != 0; }

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
    CCMenuItemSpriteExtra* m_listBtn = nullptr;
    CCMenu* m_menu = nullptr;

    // Прячущийся поиск (настройка search-bar-mode)
    CCNode* m_searchStrip = nullptr;
    CCMenuItemSpriteExtra* m_searchToggle = nullptr;
    Ref<CCArray> m_shownLevels;
    bool m_searchHidden = false;
    bool m_searchAnimating = false;
    float m_lastScrollY = 0.f;
    float m_scrollAccum = 0.f;
    std::string searchMode() const;
    void setSearchHidden(bool hidden);
    void onToggleSearch(CCObject*);
    void rebuildListView(bool keepScroll);
    void update(float dt) override;

    // Фоны-скриншоты уровней: плавно сменяются каждые 30 секунд
    std::vector<std::string> m_bgFiles;
    CCSprite* m_bgSprite = nullptr;
    size_t m_bgIndex = 0;
    void setupBackgrounds();
    CCSprite* makeBackground(std::string const& file);
    void nextBackground(float);
    void updateListButton();
    CCLabelBMFont* m_pageBtnLabel = nullptr;
    CCMenuItemSpriteExtra* m_prev = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
};

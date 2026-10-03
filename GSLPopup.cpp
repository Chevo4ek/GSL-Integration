#include "GSLPopup.hpp"
#include <algorithm>
#include <cctype>

static constexpr int PER_PAGE = 10;
static constexpr float LIST_W = 356.f;
static constexpr float LIST_H = 200.f;
static constexpr float LIST_SHIFT_X = 0.f;

static std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

CCScene* GSLListLayer::scene() {
    auto scene = CCScene::create();
    scene->addChild(GSLListLayer::create());
    return scene;
}

GSLListLayer* GSLListLayer::create() {
    auto ret = new GSLListLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool GSLListLayer::init() {
    if (!CCLayer::init()) return false;
    auto win = CCDirector::get()->getWinSize();

    auto bg = createLayerBG();
    bg->setColor({ 0, 102, 255 });
    this->addChild(bg, -2);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    this->addChild(menu, 5);

    // Назад
    auto backBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"), this, menu_selector(GSLListLayer::onBack)
    );
    backBtn->setPosition({ 25.f, win.height - 25.f });
    menu->addChild(backBtn);

    // Обновить лист с сайта
    auto refreshBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png"), this, menu_selector(GSLListLayer::onRefresh)
    );
    refreshBtn->setPosition({ win.width - 30.f, 30.f });
    menu->addChild(refreshBtn);

    // Рамка листа: по центру, чуть ниже середины
    // всё вместе (рамка + шапка + поиск) — по центру экрана
    float listBottom = (win.height - (LIST_H + 79.f)) / 2 + 16.f;
    float listTop = listBottom + LIST_H + 30.f; // вместе с зелёной шапкой

    // Вкладки списков — столбиком слева
    auto& lists = GSLManager::lists();
    for (size_t i = 0; i < lists.size(); i++) {
        auto spr = ButtonSprite::create(
            GSLManager::prettyListName(lists[i]).c_str(), 70, true, "bigFont.fnt", "GJ_button_04.png", 24.f, .5f
        );
        spr->setScale(.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLListLayer::onTab));
        btn->setTag(static_cast<int>(i));
        float tabsTop = win.height / 2 + 26.f * (lists.size() - 1) / 2 + 30.f;
        btn->setPosition({ 38.f, tabsTop - 26.f * i });
        menu->addChild(btn);
        m_tabSprites.push_back(spr);
    }

    // Пояснение для Future Classic (видно только на этой вкладке)
    m_note = CCNode::create();
    m_note->setPosition({ 38.f, win.height / 2 + 26.f * (lists.size() - 1) / 2 + 30.f - 26.f * lists.size() - 6.f });
    {
        char const* lines[] = { "Only levels with", "a GD ID are shown.", "Full list on site:" };
        float y = 0.f;
        for (auto line : lines) {
            auto lbl = CCLabelBMFont::create(line, "chatFont.fnt");
            lbl->setScale(.45f);
            lbl->setPosition({ 0.f, y });
            m_note->addChild(lbl);
            y -= 9.f;
        }
        auto noteMenu = CCMenu::create();
        noteMenu->setPosition({ 0.f, y - 8.f });
        auto siteSpr = ButtonSprite::create("Website", "goldFont.fnt", "GJ_button_01.png", .6f);
        siteSpr->setScale(.55f);
        noteMenu->addChild(CCMenuItemSpriteExtra::create(siteSpr, this, menu_selector(GSLListLayer::onSite)));
        m_note->addChild(noteMenu);
    }
    this->addChild(m_note, 5);

    // Поиск — во всю ширину рамки, сразу над ней
    float searchY = listTop + 18.f;
    m_search = TextInput::create(LIST_W - 64.f, "Search levels...", "bigFont.fnt");
    m_search->setPosition({ win.width / 2 + LIST_SHIFT_X - 32.f, searchY });
    m_search->setCallback([this](std::string const& text) { m_query = text; });
    this->addChild(m_search, 5);

    auto searchSpr = CCSprite::createWithSpriteFrameName("gj_findBtn_001.png");
    searchSpr->setScale(.65f);
    auto searchBtn = CCMenuItemSpriteExtra::create(searchSpr, this, menu_selector(GSLListLayer::onSearch));
    searchBtn->setPosition({ win.width / 2 + LIST_SHIFT_X + LIST_W / 2 - 15.f, searchY });
    menu->addChild(searchBtn);

    // Фильтр (воронка) слева от лупы: скрыть пройденные уровни
    m_filterBg = CCScale9Sprite::create("GJ_button_01.png");
    m_filterBg->setContentSize({ 40.f, 40.f });
    if (auto funnel = CCSprite::createWithSpriteFrameName("GJ_filterIcon_001.png")) {
        funnel->setScale(.75f);
        funnel->setPosition({ 20.f, 21.f });
        m_filterBg->addChild(funnel);
    }
    m_filterBg->setScale(searchSpr->getScaledContentHeight() / 40.f);
    auto filterBtn = CCMenuItemSpriteExtra::create(m_filterBg, this, menu_selector(GSLListLayer::onFilter));
    filterBtn->setPosition({ searchBtn->getPositionX() - searchSpr->getScaledContentWidth() - 4.f, searchY });
    menu->addChild(filterBtn);

    // Страницы
    auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    prevSpr->setScale(.6f);
    m_prev = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(GSLListLayer::onPrev));
    m_prev->setPosition({ win.width / 2 - LIST_W / 2 - 24.f, listBottom + LIST_H / 2 });
    menu->addChild(m_prev);

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    nextSpr->setFlipX(true);
    nextSpr->setScale(.6f);
    m_next = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(GSLListLayer::onNext));
    m_next->setPosition({ win.width / 2 + LIST_W / 2 + 24.f, listBottom + LIST_H / 2 });
    menu->addChild(m_next);

    m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_pageLabel->setScale(.4f);
    m_pageLabel->setAnchorPoint({ 1.f, .5f });
    m_pageLabel->setPosition({ win.width - 5.f, win.height - 10.f });
    this->addChild(m_pageLabel, 5);

    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setScale(.5f);
    m_status->setPosition({ win.width / 2, listBottom + LIST_H / 2 });
    this->addChild(m_status, 10);

    this->setKeypadEnabled(true);
    selectList(m_list);
    return true;
}

void GSLListLayer::onExit() {
    auto glm = GameLevelManager::get();
    if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
    CCLayer::onExit();
}

void GSLListLayer::keyBackClicked() {
    CCDirector::get()->popSceneWithTransition(.5f, PopTransition::kPopTransitionFade);
}

void GSLListLayer::onBack(CCObject*) { keyBackClicked(); }

void GSLListLayer::onTab(CCObject* sender) {
    selectList(GSLManager::lists()[static_cast<size_t>(sender->getTag())]);
}

void GSLListLayer::onPrev(CCObject*) {
    if (m_page > 0) { m_page--; loadPage(); }
}

void GSLListLayer::onNext(CCObject*) {
    int total = static_cast<int>(filtered().size());
    if ((m_page + 1) * PER_PAGE < total) { m_page++; loadPage(); }
}

void GSLListLayer::onSearch(CCObject*) {
    m_page = 0;
    loadPage();
}

void GSLListLayer::onRefresh(CCObject*) {
    GSLManager::get()->forceRefresh();
    selectList(m_list);
}

void GSLListLayer::selectList(std::string const& list) {
    m_list = list;
    m_page = 0;
    m_entries = GSLManager::get()->getList(list);

    auto& lists = GSLManager::lists();
    for (size_t i = 0; i < lists.size(); i++) {
        m_tabSprites[i]->updateBGImage(lists[i] == list ? "GJ_button_02.png" : "GJ_button_04.png");
    }
    m_note->setVisible(list == "future");
    loadPage();
}

void GSLListLayer::onSite(CCObject*) {
    web::openLinkInBrowser("https://globalshittylist.com");
}

void GSLListLayer::onFilter(CCObject*) {
    m_hideCompleted = !m_hideCompleted;
    // серая кнопка = фильтр включён
    m_filterBg->setColor(m_hideCompleted ? ccColor3B{ 150, 150, 150 } : ccColor3B{ 255, 255, 255 });
    m_page = 0;
    loadPage();
    if (m_hideCompleted && filtered().empty()) m_status->setString("All levels completed!");
}

static bool isCompleted(int id) {
    auto saved = GameLevelManager::get()->getSavedLevel(id);
    return saved && saved->m_normalPercent.value() >= 100;
}

std::vector<GSLEntry> GSLListLayer::filtered() const {
    auto q = lower(m_query);
    std::vector<GSLEntry> out;
    for (auto& e : m_entries) {
        if (!q.empty() && lower(e.name).find(q) == std::string::npos) continue;
        if (m_hideCompleted && isCompleted(e.levelId)) continue;
        out.push_back(e);
    }
    return out;
}

void GSLListLayer::loadPage() {
    auto list = filtered();
    int total = static_cast<int>(list.size());
    int from = m_page * PER_PAGE;
    int to = std::min(from + PER_PAGE, total);

    m_pageLabel->setString(total == 0 ? "" : fmt::format("{} to {} of {}", from + 1, to, total).c_str());
    m_prev->setVisible(m_page > 0);
    m_next->setVisible(to < total);

    m_pageIds.clear();
    std::string ids;
    for (int i = from; i < to; i++) {
        m_pageIds.push_back(list[i].levelId);
        if (!ids.empty()) ids += ",";
        ids += std::to_string(list[i].levelId);
    }

    if (ids.empty()) {
        showLevels(CCArray::create());
        m_status->setString(
            m_entries.empty()
                ? (GSLManager::get()->isLoading() ? "Loading list..." : "List not loaded yet")
                : "Nothing found"
        );
        return;
    }

    auto search = GJSearchObject::create(SearchType::Type19, ids);
    m_key = search->getKey();

    auto glm = GameLevelManager::get();
    if (auto stored = glm->getStoredOnlineLevels(m_key.c_str())) {
        showLevels(stored);
        return;
    }

    showLevels(CCArray::create());
    m_status->setString("Loading...");
    glm->m_levelManagerDelegate = this;
    glm->getOnlineLevels(search);
}

void GSLListLayer::loadLevelsFinished(CCArray* levels, char const* key, int) {
    if (!key || m_key != key) return;
    showLevels(levels);
}

void GSLListLayer::loadLevelsFinished(CCArray* levels, char const* key) {
    loadLevelsFinished(levels, key, 0);
}

void GSLListLayer::loadLevelsFailed(char const* key, int) {
    if (!key || m_key != key) return;
    m_status->setString("Failed to load levels");
}

void GSLListLayer::loadLevelsFailed(char const* key) {
    loadLevelsFailed(key, 0);
}

void GSLListLayer::showLevels(CCArray* levels) {
    m_status->setString("");

    // Сервер GD отдаёт уровни не по порядку — сортируем по месту в листе
    std::vector<GJGameLevel*> sorted;
    for (auto lvl : CCArrayExt<GJGameLevel*>(levels)) sorted.push_back(lvl);
    auto indexOf = [this](int id) {
        auto it = std::find(m_pageIds.begin(), m_pageIds.end(), id);
        return static_cast<int>(it - m_pageIds.begin());
    };
    std::sort(sorted.begin(), sorted.end(), [&](GJGameLevel* a, GJGameLevel* b) {
        return indexOf(a->m_levelID.value()) < indexOf(b->m_levelID.value());
    });
    auto arr = CCArray::create();
    for (auto lvl : sorted) arr->addObject(lvl);

    if (m_listLayer) m_listLayer->removeFromParent();

    auto win = CCDirector::get()->getWinSize();
    auto listView = CustomListView::create(arr, BoomListType::Level, LIST_H, LIST_W);
    m_listLayer = GJListLayer::create(
        listView, GSLManager::prettyListName(m_list).c_str(), { 191, 114, 62, 255 }, LIST_W, LIST_H, 0
    );
    m_listLayer->setPosition({ win.width / 2 - LIST_W / 2, (CCDirector::get()->getWinSize().height - (LIST_H + 79.f)) / 2 + 16.f });
    this->addChild(m_listLayer, 1);
}

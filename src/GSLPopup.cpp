#include "GSLPopup.hpp"
#include <algorithm>
#include <cctype>

static constexpr int PER_PAGE = 10;
static constexpr float LIST_W = 356.f;
static constexpr float LIST_H = 190.f;

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

// ---------- маленькие окна ----------

// Выбор списка
class GSLListPickPopup : public Popup {
public:
    static GSLListPickPopup* create(GSLListLayer* layer) {
        auto ret = new GSLListPickPopup();
        ret->m_layer = layer;
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
protected:
    GSLListLayer* m_layer = nullptr;
    bool init() {
        if (!Popup::init(220.f, 190.f)) return false;
        this->setTitle("Choose list");
        auto& lists = GSLManager::lists();
        auto menu = CCMenu::create();
        menu->setContentSize({ 200.f, 140.f });
        menu->setLayout(ColumnLayout::create()->setAxisReverse(true)->setGap(6.f));
        for (size_t i = 0; i < lists.size(); i++) {
            auto spr = ButtonSprite::create(
                GSLManager::prettyListName(lists[i]).c_str(), 150, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .6f
            );
            spr->setScale(.8f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLListPickPopup::onPick));
            btn->setTag(static_cast<int>(i));
            menu->addChild(btn);
        }
        menu->updateLayout();
        m_mainLayer->addChildAtPosition(menu, Anchor::Center, { 0.f, -10.f });
        return true;
    }
    void onPick(CCObject* sender) {
        m_layer->selectListPublic(GSLManager::lists()[static_cast<size_t>(sender->getTag())]);
        this->onClose(nullptr);
    }
};

// Переход на страницу
class GSLGoToPagePopup : public Popup {
public:
    static GSLGoToPagePopup* create(GSLListLayer* layer) {
        auto ret = new GSLGoToPagePopup();
        ret->m_layer = layer;
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
protected:
    GSLListLayer* m_layer = nullptr;
    TextInput* m_input = nullptr;
    bool init() {
        if (!Popup::init(220.f, 130.f)) return false;
        this->setTitle(fmt::format("Go to page (1-{})", m_layer->pageCount()).c_str());
        m_input = TextInput::create(120.f, "Page", "bigFont.fnt");
        m_input->setCommonFilter(CommonFilter::Uint);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, { 0.f, 5.f });
        auto spr = ButtonSprite::create("Go", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLGoToPagePopup::onGo));
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, { 0.f, 25.f });
        return true;
    }
    void onGo(CCObject*) {
        int page = std::atoi(m_input->getString().c_str());
        if (page <= 0) page = 1;
        m_layer->goToPage(page - 1);
        this->onClose(nullptr);
    }
};

// Фильтры, как в Global Demonlist
class GSLFilterPopup : public Popup {
public:
    static GSLFilterPopup* create(GSLListLayer* layer) {
        auto ret = new GSLFilterPopup();
        ret->m_layer = layer;
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
protected:
    GSLListLayer* m_layer = nullptr;
    int m_rank = 0;
    bool m_hide = false;
    bool m_onlyCreator = false;
    std::vector<std::pair<int, CCLabelBMFont*>> m_rankLabels;
    TextInput* m_creatorInput = nullptr;

    bool init() {
        if (!Popup::init(360.f, 250.f)) return false;
        this->setTitle("Search filters", "goldFont.fnt", .9f);
        m_rank = m_layer->m_rankLimit;
        m_hide = m_layer->m_hideCompleted;
        m_onlyCreator = m_layer->m_onlyCreator;

        auto size = m_mainLayer->getContentSize();
        auto box = [&](float y, float h) {
            auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
            bg->setColor({ 0, 0, 0 });
            bg->setOpacity(60);
            bg->setContentSize({ size.width - 30.f, h });
            m_mainLayer->addChildAtPosition(bg, Anchor::Top, { 0.f, y });
        };

        // Ряд 1: Top 75 / Top 150 / Top 300 / Unbounded
        box(-62.f, 40.f);
        auto rankMenu = CCMenu::create();
        rankMenu->setContentSize({ size.width - 40.f, 40.f });
        std::pair<int, char const*> ranks[] = { { 75, "Top 75" }, { 150, "Top 150" }, { 300, "Top 300" }, { 0, "Unbounded" } };
        float x = 30.f;
        for (auto& [val, name] : ranks) {
            auto lbl = CCLabelBMFont::create(name, "bigFont.fnt");
            lbl->setScale(.45f);
            auto btn = CCMenuItemSpriteExtra::create(lbl, this, menu_selector(GSLFilterPopup::onRank));
            btn->setTag(val);
            btn->setPosition({ x + lbl->getScaledContentWidth() / 2, 20.f });
            x += lbl->getScaledContentWidth() + 18.f;
            rankMenu->addChild(btn);
            m_rankLabels.push_back({ val, lbl });
        }
        rankMenu->ignoreAnchorPointForPosition(false);
        m_mainLayer->addChildAtPosition(rankMenu, Anchor::Top, { 0.f, -62.f });

        // Ряд 2: галочки
        box(-130.f, 75.f);
        auto checks = CCMenu::create();
        checks->ignoreAnchorPointForPosition(false);
        checks->setContentSize({ size.width - 40.f, 75.f });
        auto addCheck = [&](char const* text, bool on, SEL_MenuHandler cb, float y) {
            auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, .7f);
            t->toggle(on);
            t->setPosition({ 25.f, y });
            checks->addChild(t);
            auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
            lbl->setScale(.45f);
            lbl->setAnchorPoint({ 0.f, .5f });
            lbl->setPosition({ 45.f, y });
            checks->addChild(lbl);
        };
        addCheck("Hide completed", m_hide, menu_selector(GSLFilterPopup::onHide), 55.f);
        addCheck("Created by", m_onlyCreator, menu_selector(GSLFilterPopup::onCreator), 20.f);
        m_mainLayer->addChildAtPosition(checks, Anchor::Top, { 0.f, -130.f });

        m_creatorInput = TextInput::create(130.f, "Creator name", "bigFont.fnt");
        m_creatorInput->setString(m_layer->m_creator);
        m_creatorInput->setScale(.8f);
        m_mainLayer->addChildAtPosition(m_creatorInput, Anchor::Top, { 85.f, -147.f });

        // Apply
        auto spr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", .9f);
        auto apply = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLFilterPopup::onApply));
        m_buttonMenu->addChildAtPosition(apply, Anchor::Bottom, { 0.f, 28.f });

        updateRanks();
        return true;
    }
    void updateRanks() {
        for (auto& [val, lbl] : m_rankLabels) {
            lbl->setColor(val == m_rank ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 120, 120, 120 });
        }
    }
    void onRank(CCObject* sender) { m_rank = sender->getTag(); updateRanks(); }
    void onHide(CCObject*) { m_hide = !m_hide; }
    void onCreator(CCObject*) { m_onlyCreator = !m_onlyCreator; }
    void onApply(CCObject*) {
        m_layer->m_rankLimit = m_rank;
        m_layer->m_hideCompleted = m_hide;
        m_layer->m_onlyCreator = m_onlyCreator;
        m_layer->m_creator = m_creatorInput->getString();
        m_layer->applyFilters();
        this->onClose(nullptr);
    }
};

// ---------- экран листа ----------

// Высота всего блока: поиск 30 + отступ 10 + шапка 30 + список + нижняя рамка 16
static constexpr float BLOCK_H = 30.f + 10.f + 30.f + LIST_H + 16.f;

static float listBottomY() {
    return (CCDirector::get()->getWinSize().height - BLOCK_H) / 2 + 16.f;
}

static CCScale9Sprite* squareBg(char const* texture) {
    auto bg = CCScale9Sprite::create(texture);
    bg->setContentSize({ 40.f, 40.f });
    bg->setScale(.75f);
    return bg;
}

static CCMenuItemSpriteExtra* squareBtn(char const* texture, char const* text, CCObject* target, SEL_MenuHandler cb, CCLabelBMFont** outLabel = nullptr) {
    auto bg = squareBg(texture);
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->limitLabelWidth(30.f, .6f, .1f);
    lbl->setPosition({ 20.f, 21.f });
    bg->addChild(lbl);
    if (outLabel) *outLabel = lbl;
    return CCMenuItemSpriteExtra::create(bg, target, cb);
}

// Кнопка с кубиком (рисуем сами: белый кубик с пятью точками)
static CCMenuItemSpriteExtra* diceBtn(char const* texture, CCObject* target, SEL_MenuHandler cb) {
    auto bg = squareBg(texture);
    auto dice = CCDrawNode::create();
    float r = 11.f;
    CCPoint box[] = { { -r, -r }, { r, -r }, { r, r }, { -r, r } };
    dice->drawPolygon(box, 4, { 1.f, 1.f, 1.f, 1.f }, 1.5f, { 0.f, 0.f, 0.f, 1.f });
    float d = 5.5f;
    CCPoint dots[] = { { 0.f, 0.f }, { -d, -d }, { d, d }, { -d, d }, { d, -d } };
    for (auto& p : dots) dice->drawDot(p, 2.f, { 0.f, 0.f, 0.f, 1.f });
    dice->setPosition({ 20.f, 21.f });
    bg->addChild(dice);
    return CCMenuItemSpriteExtra::create(bg, target, cb);
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

    float listBottom = listBottomY();
    float listTop = listBottom + LIST_H + 30.f;
    float midY = listBottom + LIST_H / 2;

    // Справа сверху: счётчик, выбор списка, кнопки страниц
    m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_pageLabel->setScale(.4f);
    m_pageLabel->setAnchorPoint({ 1.f, .5f });
    m_pageLabel->setPosition({ win.width - 5.f, win.height - 10.f });
    this->addChild(m_pageLabel, 5);

    m_listBtnSpr = ButtonSprite::create("Classic", 90, true, "bigFont.fnt", "GJ_button_04.png", 26.f, .5f);
    m_listBtnSpr->setScale(.7f);
    auto listBtn = CCMenuItemSpriteExtra::create(m_listBtnSpr, this, menu_selector(GSLListLayer::onPickList));
    listBtn->setPosition({ win.width - 42.f, win.height - 30.f });
    menu->addChild(listBtn);

    auto pageBtn = squareBtn("GJ_button_02.png", "1", this, menu_selector(GSLListLayer::onGoToPage), &m_pageBtnLabel);
    pageBtn->setPosition({ win.width - 22.f, win.height - 62.f });
    menu->addChild(pageBtn);

    auto randBtn = diceBtn("GJ_button_03.png", this, menu_selector(GSLListLayer::onRandomPage));
    randBtn->setPosition({ win.width - 22.f, win.height - 94.f });
    menu->addChild(randBtn);

    auto lastBtn = squareBtn("GJ_button_06.png", ">>", this, menu_selector(GSLListLayer::onLastPage));
    lastBtn->setPosition({ win.width - 22.f, win.height - 126.f });
    menu->addChild(lastBtn);

    // Пояснение для Future Classic — слева
    m_note = CCNode::create();
    m_note->setPosition({ 50.f, midY + 20.f });
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

    // Поиск — шириной с рамку, с отступом над ней; справа фильтр и лупа
    float searchY = listTop + 10.f + 15.f;
    auto searchSpr = CCSprite::createWithSpriteFrameName("gj_findBtn_001.png");
    searchSpr->setScale(.65f);
    float btnW = searchSpr->getScaledContentWidth();
    float rightX = win.width / 2 + LIST_W / 2;

    auto searchBtn = CCMenuItemSpriteExtra::create(searchSpr, this, menu_selector(GSLListLayer::onSearch));
    searchBtn->setPosition({ rightX - btnW / 2, searchY });
    menu->addChild(searchBtn);

    m_filterBg = CCScale9Sprite::create("GJ_button_01.png");
    m_filterBg->setContentSize({ 40.f, 40.f });
    if (auto funnel = CCSprite::createWithSpriteFrameName("GJ_filterIcon_001.png")) {
        funnel->setScale(.75f);
        funnel->setPosition({ 20.f, 21.f });
        m_filterBg->addChild(funnel);
    }
    m_filterBg->setScale(searchSpr->getScaledContentHeight() / 40.f);
    auto filterBtn = CCMenuItemSpriteExtra::create(m_filterBg, this, menu_selector(GSLListLayer::onFilter));
    filterBtn->setPosition({ rightX - btnW * 1.5f - 4.f, searchY });
    menu->addChild(filterBtn);

    float inputW = LIST_W - btnW * 2 - 12.f;
    m_search = TextInput::create(inputW, "Search levels...", "bigFont.fnt");
    m_search->setPosition({ win.width / 2 - LIST_W / 2 + inputW / 2, searchY });
    m_search->setCallback([this](std::string const& text) { m_query = text; });
    this->addChild(m_search, 5);

    // Стрелки страниц — одинаковые, на одном расстоянии от рамки
    float arrowGap = 32.f;
    auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    prevSpr->setScale(.7f);
    m_prev = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(GSLListLayer::onPrev));
    m_prev->setPosition({ win.width / 2 - LIST_W / 2 - arrowGap, midY });
    menu->addChild(m_prev);

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    nextSpr->setFlipX(true);
    nextSpr->setScale(.7f);
    m_next = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(GSLListLayer::onNext));
    m_next->setPosition({ win.width / 2 + LIST_W / 2 + arrowGap, midY });
    menu->addChild(m_next);

    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setScale(.5f);
    m_status->setPosition({ win.width / 2, midY });
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

void GSLListLayer::onPickList(CCObject*) { GSLListPickPopup::create(this)->show(); }
void GSLListLayer::onGoToPage(CCObject*) { GSLGoToPagePopup::create(this)->show(); }

int GSLListLayer::pageCount() const {
    int total = static_cast<int>(filtered().size());
    return std::max(1, (total + PER_PAGE - 1) / PER_PAGE);
}

void GSLListLayer::goToPage(int page) {
    m_page = std::clamp(page, 0, pageCount() - 1);
    loadPage();
}

void GSLListLayer::onRandomPage(CCObject*) {
    int n = pageCount();
    if (n <= 1) return;
    int p = m_page;
    while (p == m_page) p = std::rand() % n;
    goToPage(p);
}

void GSLListLayer::onLastPage(CCObject*) { goToPage(pageCount() - 1); }

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

    m_listBtnSpr->setString(GSLManager::prettyListName(list).c_str());
    m_note->setVisible(list == "future");
    loadPage();
}

void GSLListLayer::onSite(CCObject*) {
    web::openLinkInBrowser("https://globalshittylist.com");
}

void GSLListLayer::onFilter(CCObject*) { GSLFilterPopup::create(this)->show(); }

void GSLListLayer::applyFilters() {
    bool active = m_rankLimit > 0 || m_hideCompleted || (m_onlyCreator && !m_creator.empty());
    // серая воронка = фильтр включён
    m_filterBg->setColor(active ? ccColor3B{ 150, 150, 150 } : ccColor3B{ 255, 255, 255 });
    m_page = 0;
    loadPage();
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
        if (m_rankLimit > 0 && e.rank > m_rankLimit) continue;
        if (m_hideCompleted && isCompleted(e.levelId)) continue;
        if (m_onlyCreator && !m_creator.empty() && lower(e.author).find(lower(m_creator)) == std::string::npos) continue;
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
    m_pageBtnLabel->setString(std::to_string(m_page + 1).c_str());
    m_pageBtnLabel->limitLabelWidth(30.f, .6f, .1f);
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
    m_listLayer->setPosition({ win.width / 2 - LIST_W / 2, listBottomY() });
    this->addChild(m_listLayer, 1);
}

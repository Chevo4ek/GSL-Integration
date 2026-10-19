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

// Фильтры — как на сайте (Category, Length, Only new, Quality, Sort by),
// но в стиле GD. Плюс Completed by / Created by.
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

    // выбранные значения (применяются по Apply)
    std::string m_category;
    int m_length = 0; // 0 = все, иначе бит длины
    bool m_onlyNew = false;
    std::string m_quality;
    int m_sort = 0;
    bool m_completedBy = false, m_createdBy = false;

    std::vector<std::string> m_categories; // "" = All
    std::vector<std::string> m_qualities;  // "" = All
    std::vector<std::pair<std::string, ButtonSprite*>> m_categoryBtns;
    std::vector<std::pair<int, ButtonSprite*>> m_lengthBtns;
    CCLabelBMFont* m_qualityLabel = nullptr;
    CCLabelBMFont* m_sortLabel = nullptr;
    TextInput* m_victorInput = nullptr;
    TextInput* m_creatorInput = nullptr;

    static constexpr float W = 440.f, H = 290.f;

    CCLabelBMFont* header(char const* text, float y) {
        auto lbl = CCLabelBMFont::create(text, "goldFont.fnt");
        lbl->setScale(.45f);
        lbl->setAnchorPoint({ 0.f, .5f });
        m_mainLayer->addChildAtPosition(lbl, Anchor::TopLeft, { 18.f, y });
        return lbl;
    }

    CCScale9Sprite* box(float y, float h) {
        auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(60);
        bg->setContentSize({ W - 24.f, h });
        m_mainLayer->addChildAtPosition(bg, Anchor::Top, { 0.f, y });
        return bg;
    }

    // ряд кнопок-«таблеток», слева направо, ужимается, если не помещается
    template <class T>
    void chipRow(float y, std::vector<std::pair<T, std::string>> const& items, SEL_MenuHandler cb,
                 std::vector<std::pair<T, ButtonSprite*>>& out) {
        auto menu = CCMenu::create();
        menu->setContentSize({ W - 36.f, 26.f });
        menu->ignoreAnchorPointForPosition(false);
        menu->setLayout(RowLayout::create()->setGap(5.f)->setAxisAlignment(AxisAlignment::Start)->setAutoScale(true));
        for (size_t i = 0; i < items.size(); i++) {
            auto spr = ButtonSprite::create(items[i].second.c_str(), "bigFont.fnt", "GJ_button_04.png", .6f);
            spr->setScale(.6f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
            btn->setTag(static_cast<int>(i));
            menu->addChild(btn);
            out.push_back({ items[i].first, spr });
        }
        menu->updateLayout();
        m_mainLayer->addChildAtPosition(menu, Anchor::Top, { 0.f, y });
    }

    // «◀ значение ▶» как в настройках GD
    CCLabelBMFont* selector(float x, float y, char const* title, SEL_MenuHandler prev, SEL_MenuHandler next) {
        auto t = CCLabelBMFont::create(title, "goldFont.fnt");
        t->setScale(.4f);
        m_mainLayer->addChildAtPosition(t, Anchor::Top, { x, y + 14.f });

        auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(90);
        bg->setContentSize({ 150.f, 24.f });
        m_mainLayer->addChildAtPosition(bg, Anchor::Top, { x, y - 4.f });

        auto value = CCLabelBMFont::create("", "bigFont.fnt");
        m_mainLayer->addChildAtPosition(value, Anchor::Top, { x, y - 4.f });

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        auto mk = [&](bool flip, SEL_MenuHandler cb, float dx) {
            auto a = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            a->setScale(.35f);
            a->setFlipX(flip);
            auto b = CCMenuItemSpriteExtra::create(a, this, cb);
            // те же координаты, что у Anchor::Top: середина сверху + смещение
            b->setPosition({ W / 2 + x + dx, H + y - 4.f });
            menu->addChild(b);
        };
        mk(false, prev, -88.f);
        mk(true, next, 88.f);
        m_mainLayer->addChild(menu, 5);
        return value;
    }

    bool init() {
        if (!Popup::init(W, H)) return false;
        this->setTitle("Filters", "goldFont.fnt", .9f);

        m_category = m_layer->m_category;
        m_length = m_layer->m_lengthMask;
        m_onlyNew = m_layer->m_onlyNew;
        m_quality = m_layer->m_quality;
        m_sort = m_layer->m_sort;
        m_completedBy = m_layer->m_completedBy;
        m_createdBy = m_layer->m_onlyCreator;

        // варианты берём из самого листа: какие категории/качества реально есть
        auto list = GSLManager::get()->getList(m_layer->currentList());
        m_categories.push_back("");
        for (auto& c : GSLManager::categoryOrder()) {
            if (std::any_of(list.begin(), list.end(), [&](auto& e) { return GSLManager::categoryOf(e) == c; }))
                m_categories.push_back(c);
        }
        for (auto& e : list) {
            auto c = GSLManager::categoryOf(e);
            if (std::find(m_categories.begin(), m_categories.end(), c) == m_categories.end()) m_categories.push_back(c);
        }
        m_qualities.push_back("");
        for (auto& e : list) {
            if (!e.quality.empty() && std::find(m_qualities.begin(), m_qualities.end(), e.quality) == m_qualities.end())
                m_qualities.push_back(e.quality);
        }
        bool hasNew = std::any_of(list.begin(), list.end(), [](auto& e) { return e.isNew; });

        // CATEGORY
        header("CATEGORY", -36.f);
        std::vector<std::pair<std::string, std::string>> cats;
        for (auto& c : m_categories) cats.push_back({ c, c.empty() ? "All" : c });
        chipRow(-56.f, cats, menu_selector(GSLFilterPopup::onCategory), m_categoryBtns);

        // LENGTH
        header("LENGTH", -80.f);
        std::vector<std::pair<int, std::string>> lens = {
            { 0, "All lengths" }, { 1 << 2, "Medium" }, { 1 << 3, "Long" }, { 1 << 4, "XL" }
        };
        chipRow(-100.f, lens, menu_selector(GSLFilterPopup::onLength), m_lengthBtns);

        // Only new · Completed by · Created by
        box(-138.f, 40.f);
        auto checks = CCMenu::create();
        checks->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(checks, 5);
        auto top = m_mainLayer->getContentSize().height;
        auto addCheck = [&](float x, char const* text, bool on, SEL_MenuHandler cb, bool enabled) {
            auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, .55f);
            t->toggle(on);
            t->setEnabled(enabled);
            if (!enabled) t->setOpacity(100);
            t->setPosition({ x, top - 138.f });
            checks->addChild(t);
            auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
            lbl->setScale(.35f);
            lbl->setAnchorPoint({ 0.f, .5f });
            lbl->setPosition({ x + 13.f, top - 138.f });
            if (!enabled) lbl->setOpacity(100);
            checks->addChild(lbl);
        };
        addCheck(28.f, "Only new", m_onlyNew && hasNew, menu_selector(GSLFilterPopup::onNew), hasNew);
        addCheck(130.f, "Completed by", m_completedBy, menu_selector(GSLFilterPopup::onCompleted), true);
        addCheck(290.f, "Created by", m_createdBy, menu_selector(GSLFilterPopup::onCreated), true);

        m_victorInput = TextInput::create(150.f, "Username", "bigFont.fnt");
        m_victorInput->setString(m_layer->m_victor);
        m_victorInput->setScale(.55f);
        m_mainLayer->addChildAtPosition(m_victorInput, Anchor::TopLeft, { 182.f, -160.f });
        m_creatorInput = TextInput::create(150.f, "Creator Name", "bigFont.fnt");
        m_creatorInput->setString(m_layer->m_creator);
        m_creatorInput->setScale(.55f);
        m_mainLayer->addChildAtPosition(m_creatorInput, Anchor::TopLeft, { 336.f, -160.f });

        // Quality · Sort by
        m_qualityLabel = selector(-100.f, -200.f, "Quality",
            menu_selector(GSLFilterPopup::onQualityPrev), menu_selector(GSLFilterPopup::onQualityNext));
        m_sortLabel = selector(100.f, -200.f, "Sort by",
            menu_selector(GSLFilterPopup::onSortPrev), menu_selector(GSLFilterPopup::onSortNext));

        auto spr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", .9f);
        auto apply = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLFilterPopup::onApply));
        m_buttonMenu->addChildAtPosition(apply, Anchor::Bottom, { 0.f, 24.f });

        refresh();
        return true;
    }

    static char const* sortName(int s) {
        switch (s) {
            case 1: return "Reverse rank";
            case 2: return "Name A-Z";
            case 3: return "Points";
            default: return "List rank";
        }
    }

    void refresh() {
        for (auto& [val, spr] : m_categoryBtns)
            spr->updateBGImage(val == m_category ? "GJ_button_01.png" : "GJ_button_04.png");
        for (auto& [val, spr] : m_lengthBtns)
            spr->updateBGImage(val == m_length ? "GJ_button_01.png" : "GJ_button_04.png");
        m_qualityLabel->setString(m_quality.empty()
            ? (m_qualities.size() > 1 ? "All quality tiers" : "No data") : m_quality.c_str());
        m_qualityLabel->limitLabelWidth(120.f, .4f, .1f);
        m_sortLabel->setString(sortName(m_sort));
        m_sortLabel->limitLabelWidth(120.f, .4f, .1f);
    }

    void onCategory(CCObject* s) { m_category = m_categories[static_cast<size_t>(s->getTag())]; refresh(); }
    void onLength(CCObject* s) {
        static int const vals[] = { 0, 1 << 2, 1 << 3, 1 << 4 };
        m_length = vals[s->getTag()];
        refresh();
    }
    void stepQuality(int d) {
        auto it = std::find(m_qualities.begin(), m_qualities.end(), m_quality);
        int i = it == m_qualities.end() ? 0 : static_cast<int>(it - m_qualities.begin());
        int n = static_cast<int>(m_qualities.size());
        m_quality = m_qualities[static_cast<size_t>(((i + d) % n + n) % n)];
        refresh();
    }
    void onQualityPrev(CCObject*) { stepQuality(-1); }
    void onQualityNext(CCObject*) { stepQuality(1); }
    void onSortPrev(CCObject*) { m_sort = (m_sort + 3) % 4; refresh(); }
    void onSortNext(CCObject*) { m_sort = (m_sort + 1) % 4; refresh(); }
    void onNew(CCObject*) { m_onlyNew = !m_onlyNew; }
    void onCompleted(CCObject*) { m_completedBy = !m_completedBy; }
    void onCreated(CCObject*) { m_createdBy = !m_createdBy; }

    void onApply(CCObject*) {
        m_layer->m_category = m_category;
        m_layer->m_lengthMask = m_length;
        m_layer->m_onlyNew = m_onlyNew;
        m_layer->m_quality = m_quality;
        m_layer->m_sort = m_sort;
        m_layer->m_completedBy = m_completedBy;
        m_layer->m_victor = m_victorInput->getString();
        m_layer->m_onlyCreator = m_createdBy;
        m_layer->m_creator = m_creatorInput->getString();
        m_layer->applyFilters();
        this->onClose(nullptr);
    }
};

// ---------- экран листа ----------

// Полоса поиска внутри рамки, над списком (как в Global Demonlist)
static constexpr float SEARCH_H = 30.f;
// Цвета списка: синий фон карточек и тёмная полоска между ними
static constexpr ccColor4B LIST_BG = { 0, 80, 200, 255 };
static constexpr ccColor4B LIST_SEPARATOR = { 0, 40, 120, 255 };

// Каждой видимой карточке — синий фон и полоску снизу (карточки создаются
// при прокрутке, поэтому проверяем каждый кадр; это дёшево — их на экране 2–3)
static void styleCells(BoomListView* listView) {
    if (!listView || !listView->m_tableView || !listView->m_tableView->m_contentLayer) return;
    auto content = listView->m_tableView->m_contentLayer;
    if (!content->getChildren()) return;
    for (auto node : CCArrayExt<CCNode*>(content->getChildren())) {
        auto cell = typeinfo_cast<TableViewCell*>(node);
        if (!cell) continue;
        if (auto bg = cell->m_backgroundLayer) {
            bg->setColor({ LIST_BG.r, LIST_BG.g, LIST_BG.b });
            bg->setOpacity(LIST_BG.a);
            bg->setVisible(true);
        }
        if (!cell->getChildByID("separator"_spr)) {
            auto line = CCLayerColor::create(LIST_SEPARATOR, cell->m_width > 0 ? cell->m_width : LIST_W, 1.f);
            line->setID("separator"_spr);
            cell->addChild(line, 100);
        }
    }
}
// Высота всего блока: шапка 30 + полоса поиска + список + нижняя рамка 16
static constexpr float BLOCK_H = 30.f + SEARCH_H + LIST_H + 16.f;

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

bool GSLListLayer::init() {
    if (!CCLayer::init()) return false;
    auto win = CCDirector::get()->getWinSize();

    auto bg = createLayerBG();
    bg->setColor({ 0, 102, 255 });
    this->addChild(bg, -2);
    setupBackgrounds();

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
    float midY = listBottom + LIST_H / 2;

    // Справа сверху: счётчик, выбор списка, кнопки страниц
    m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_pageLabel->setScale(.4f);
    m_pageLabel->setAnchorPoint({ 1.f, .5f });
    m_pageLabel->setPosition({ win.width - 5.f, win.height - 10.f });
    this->addChild(m_pageLabel, 5);

    m_menu = menu; // кнопка выбора списка создаётся в updateListButton

    auto pageBtn = squareBtn("GJ_button_02.png", "1", this, menu_selector(GSLListLayer::onGoToPage), &m_pageBtnLabel);
    pageBtn->setPosition({ win.width - 22.f, win.height - 74.f });
    menu->addChild(pageBtn);

    // Рандомайзер и «в конец» — как в Global Demonlist Integration
    auto randSpr = CCSprite::create("gsl_randomBtn.png"_spr);
    randSpr->setScale(30.f / randSpr->getContentWidth());
    auto randBtn = CCMenuItemSpriteExtra::create(randSpr, this, menu_selector(GSLListLayer::onRandomPage));
    randBtn->setPosition({ win.width - 22.f, win.height - 107.f });
    menu->addChild(randBtn);

    auto lastArrow = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
    lastArrow->setFlipX(true);
    auto otherArrow = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
    otherArrow->setFlipX(true);
    otherArrow->setPosition(lastArrow->getContentSize() / 2.f + CCPoint{ 20.f, 0.f });
    lastArrow->addChild(otherArrow);
    lastArrow->setScale(.4f);
    auto lastBtn = CCMenuItemSpriteExtra::create(lastArrow, this, menu_selector(GSLListLayer::onLastPage));
    lastBtn->setPosition({ win.width - 26.f, win.height - 137.f });
    menu->addChild(lastBtn);

    // Синяя «i» в левом нижнем углу: что такое GSL + ссылка на сайт
    auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    auto infoBtn = CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(GSLListLayer::onInfo));
    infoBtn->setPosition({ 25.f, 25.f });
    menu->addChild(infoBtn);

    // Стрелочка к «i» на Upcoming Levels — пока пояснение не прочитано
    m_note = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    m_note->setScale(.5f);
    m_note->setPosition({ 62.f, 25.f });
    m_note->runAction(CCRepeatForever::create(CCSequence::create(
        CCEaseSineInOut::create(CCMoveBy::create(.45f, { 8.f, 0.f })),
        CCEaseSineInOut::create(CCMoveBy::create(.45f, { -8.f, 0.f })),
        nullptr
    )));
    m_note->setVisible(false);
    this->addChild(m_note, 5);

    // Рамка списка создаётся один раз; при смене страницы меняется только список внутри
    m_listLayer = GJListLayer::create(
        nullptr, GSLManager::prettyListName(m_list).c_str(), { 191, 114, 62, 255 }, LIST_W, LIST_H + SEARCH_H, 0
    );
    m_listLayer->setPosition({ win.width / 2 - LIST_W / 2, listBottom });
    this->addChild(m_listLayer, 1);

    // Свой непрозрачный синий фон списка — не зависит от текстур-паков и фоновых картинок
    auto listBg = CCLayerColor::create(LIST_BG, LIST_W, LIST_H + SEARCH_H);
    listBg->setID("list-bg"_spr);
    m_listLayer->addChild(listBg);

    // Поиск — полоса внутри рамки над списком, как в игре и в Global Demonlist
    {
        // якорь сверху — при скрытии полоса плавно сжимается к заголовку
        auto stripMenu = CCMenu::create();
        stripMenu->ignoreAnchorPointForPosition(false);
        stripMenu->setAnchorPoint({ 0.f, 1.f });
        stripMenu->setContentSize({ LIST_W, SEARCH_H });
        stripMenu->setPosition({ 0.f, LIST_H + SEARCH_H });
        stripMenu->setID("search-bar-menu"_spr);
        m_listLayer->addChild(stripMenu);
        m_searchStrip = stripMenu;

        auto strip = CCLayerColor::create({ 194, 114, 62, 255 }, LIST_W, SEARCH_H);
        stripMenu->addChild(strip, -1);

        float midY = SEARCH_H / 2;
        auto searchSpr = CCSprite::createWithSpriteFrameName("gj_findBtn_001.png");
        searchSpr->setScale(.7f);
        float btnW = searchSpr->getScaledContentWidth();
        auto searchBtn = CCMenuItemSpriteExtra::create(searchSpr, this, menu_selector(GSLListLayer::onSearch));
        searchBtn->setPosition({ LIST_W - 4.f - btnW / 2, midY });
        stripMenu->addChild(searchBtn);

        m_filterBg = CCScale9Sprite::create("GJ_button_01.png");
        m_filterBg->setContentSize({ 40.f, 40.f });
        if (auto funnel = CCSprite::createWithSpriteFrameName("GJ_filterIcon_001.png")) {
            funnel->setScale(.75f);
            funnel->setPosition({ 20.f, 21.f });
            m_filterBg->addChild(funnel);
        }
        m_filterBg->setScale(searchSpr->getScaledContentHeight() / 40.f);
        auto filterBtn = CCMenuItemSpriteExtra::create(m_filterBg, this, menu_selector(GSLListLayer::onFilter));
        filterBtn->setPosition({ LIST_W - 4.f - btnW * 1.5f - 4.f, midY });
        stripMenu->addChild(filterBtn);

        float inputW = LIST_W - btnW * 2 - 24.f;
        m_search = TextInput::create(inputW, "Search levels...");
        m_search->setTextAlign(TextInputAlign::Left);
        auto input = m_search->getInputNode();
        input->setLabelPlaceholderScale(.4f);
        input->setMaxLabelScale(.4f);
        auto inputBg = m_search->getBGSprite();
        inputBg->setContentSize({ inputW * 2, 40.f });
        inputBg->setScale(.5f);
        m_search->setPosition({ 6.f + inputW / 2, midY });
        m_search->setCallback([this](std::string const& text) { m_query = text; });
        stripMenu->addChild(m_search);
    }

    // Маленькая стрелочка: открыть/закрыть поиск (нет в режиме "Always shown")
    if (searchMode() != "Always shown") {
        auto arrowMenu = CCMenu::create();
        arrowMenu->setPosition({ 0.f, 0.f });
        m_listLayer->addChild(arrowMenu, 20);

        auto arrow = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        auto holder = CCNode::create();
        holder->setContentSize({ 16.f, 16.f });
        auto bgDot = CCScale9Sprite::create("GJ_button_04.png");
        bgDot->setContentSize({ 32.f, 32.f });
        bgDot->setScale(.5f);
        bgDot->setPosition({ 8.f, 8.f });
        holder->addChild(bgDot);
        arrow->setScale(10.f / arrow->getContentHeight());
        arrow->setPosition({ 8.f, 8.f });
        arrow->setTag(1);
        holder->addChild(arrow);
        m_searchToggle = CCMenuItemSpriteExtra::create(holder, this, menu_selector(GSLListLayer::onToggleSearch));
        m_searchToggle->setPosition({ LIST_W / 2, LIST_H + SEARCH_H - 1.f });
        arrowMenu->addChild(m_searchToggle);
    }

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
    if (searchMode() == "Arrow only") {
        m_searchHidden = true;
        m_searchStrip->setVisible(false);
        m_searchStrip->setScaleY(0.f);
        if (m_searchToggle) m_searchToggle->getNormalImage()->getChildByTag(1)->setRotation(-90.f);
    }
    else if (m_searchToggle) {
        m_searchToggle->getNormalImage()->getChildByTag(1)->setRotation(90.f);
    }
    this->scheduleUpdate();
    // лист по умолчанию — из настроек
    auto def = Mod::get()->getSettingValue<std::string>("default-list");
    for (auto& l : GSLManager::lists()) {
        if (GSLManager::prettyListName(l) == def) m_list = l;
    }
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

    updateListButton();
    if (auto title = typeinfo_cast<CCLabelBMFont*>(m_listLayer->getChildByID("title"))) {
        title->setString(GSLManager::prettyListName(list).c_str());
        title->limitLabelWidth(280.f, .8f, .1f);
    }
    m_note->setVisible(list == "future" && !Mod::get()->getSavedValue<bool>("future-info-read"));
    loadPage();
}

// Квадратная зелёная кнопка: кубок сверху, название списка снизу.
// Название всегда помещается внутри квадрата (длинное — в две строки).
void GSLListLayer::updateListButton() {
    if (m_listBtn) m_listBtn->removeFromParent();
    auto win = CCDirector::get()->getWinSize();

    float size = 46.f;
    float pad = 5.f;
    float inner = size - pad * 2;
    auto bg = CCScale9Sprite::create("GJ_button_01.png");
    bg->setContentSize({ size, size });

    // слова названия — каждое отдельной строкой, все одним масштабом
    auto words = utils::string::split(GSLManager::prettyListName(m_list), " ");
    float textH = 16.f; // максимальная высота надписи
    float scale = .5f;
    std::vector<CCLabelBMFont*> lines;
    for (auto& w : words) {
        if (w.empty()) continue;
        auto lbl = CCLabelBMFont::create(w.c_str(), "bigFont.fnt");
        scale = std::min(scale, inner / lbl->getContentWidth());
        lines.push_back(lbl);
    }
    // кубок + надпись — одна группа, вся группа ровно по центру кнопки
    // (центр чуть выше середины: у кнопки толще нижний край)
    float cupH = 17.f, gap = 3.f;
    float lineH = 0.f, blockH = 0.f;
    if (!lines.empty()) {
        lineH = lines[0]->getContentHeight() * .8f;
        scale = std::min(scale, textH / (lineH * lines.size()));
        blockH = lineH * scale * lines.size();
    }
    float total = cupH + (lines.empty() ? 0.f : gap + blockH);
    float bottom = size / 2 + 1.f - total / 2;

    float y = bottom + blockH - lineH * scale / 2;
    for (auto lbl : lines) {
        lbl->setScale(scale);
        lbl->setPosition({ size / 2, y });
        bg->addChild(lbl);
        y -= lineH * scale;
    }

    if (auto icon = CCSprite::createWithSpriteFrameName("rankIcon_1_001.png")) {
        icon->setScale(cupH / icon->getContentHeight());
        icon->setPosition({ size / 2, bottom + total - cupH / 2 });
        bg->addChild(icon);
    }

    bg->setScale(.75f);
    m_listBtn = CCMenuItemSpriteExtra::create(bg, this, menu_selector(GSLListLayer::onPickList));
    m_listBtn->setPosition({ win.width - 22.f, win.height - 34.f });
    m_menu->addChild(m_listBtn);
}

void GSLListLayer::onInfo(CCObject*) {
    std::string text =
        "The <cr>hardest</c> shitty levels,\nranked by the <cy>community</c>.\n"
        "Track <cg>records</c>, discover <cl>victors</c> and climb the <co>leaderboard</c> "
        "of the Geometry Dash shitty-level scene.";
    bool future = m_list == "future";
    if (future) {
        text += "\n\n<co>UPCOMING LEVELS:</c> only levels that already have a "
                "<cy>GD ID</c> are shown here. <co>The full list is on the site!</c>";
    }
    Ref<GSLListLayer> self = this;
    createQuickPopup(
        "Global Shitty List", text, "OK", "Website", 360.f,
        [self, future](FLAlertLayer*, bool website) {
            if (future) {
                Mod::get()->setSavedValue("future-info-read", true);
                self->m_note->setVisible(false);
            }
            if (website) web::openLinkInBrowser("https://globalshittylist.com");
        }
    );
}

void GSLListLayer::onSite(CCObject*) {
    web::openLinkInBrowser("https://globalshittylist.com");
}

void GSLListLayer::onFilter(CCObject*) { GSLFilterPopup::create(this)->show(); }

void GSLListLayer::applyFilters() {
    bool active = !m_category.empty() || !m_quality.empty() || m_onlyNew || m_sort != 0
        || m_lengthMask != 0 || m_completedBy
        || (m_onlyCreator && !m_creator.empty());
    // серая воронка = фильтр включён
    m_filterBg->setColor(active ? ccColor3B{ 150, 150, 150 } : ccColor3B{ 255, 255, 255 });
    m_page = 0;
    loadPage();
}

// useGD = false — без фильтров по длине/звёздам (чтобы узнать, чьи данные докачать)
std::vector<GSLEntry> GSLListLayer::filtered(bool useGD) const {
    auto q = lower(m_query);
    auto creator = lower(m_creator);
    std::string victor = m_victor;
    if (m_completedBy && victor.empty()) victor = GJAccountManager::get()->m_username;
    victor = lower(victor);
    auto gsl = GSLManager::get();

    std::vector<GSLEntry> out;
    for (auto& e : m_entries) {
        if (!q.empty() && lower(e.name).find(q) == std::string::npos) continue;
        if (!m_category.empty() && GSLManager::categoryOf(e) != m_category) continue;
        if (!m_quality.empty() && e.quality != m_quality) continue;
        if (m_onlyNew && !e.isNew) continue;
        if (m_onlyCreator && !creator.empty() && lower(e.author).find(creator) == std::string::npos) continue;
        if (m_completedBy) {
            bool beaten = std::any_of(e.victors.begin(), e.victors.end(),
                [&](auto& v) { return lower(v) == victor; });
            if (!beaten) continue;
        }
        if (useGD && needsGDInfo()) {
            auto info = gsl->gdInfo(e.levelId);
            if (!info) continue;
            if (m_lengthMask && (info->length < 0 || !(m_lengthMask & (1 << info->length)))) continue;
        }
        out.push_back(e);
    }
    // Sort by (лист и так отсортирован по месту)
    if (m_sort == 1) std::reverse(out.begin(), out.end());
    else if (m_sort == 2) std::stable_sort(out.begin(), out.end(), [](auto& a, auto& b) { return lower(a.name) < lower(b.name); });
    else if (m_sort == 3) std::stable_sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.points > b.points; });
    return out;
}

void GSLListLayer::loadPage() {
    // для фильтра по длине нужны данные с серверов GD — докачиваем недостающие
    if (needsGDInfo()) {
        auto gsl = GSLManager::get();
        std::vector<int> missing;
        for (auto& e : filtered(false)) if (!gsl->gdInfo(e.levelId)) missing.push_back(e.levelId);
        if (!missing.empty()) {
            showLevels(CCArray::create());
            m_status->setString(fmt::format("Loading level info ({})...", missing.size()).c_str());
            m_pageLabel->setString("");
            Ref<GSLListLayer> self = this;
            gsl->requestGDInfo(missing, [self] {
                if (self->isRunning()) self->loadPage();
            });
            return;
        }
    }

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
    for (auto lvl : CCArrayExt<GJGameLevel*>(levels)) {
        sorted.push_back(lvl);
        GSLManager::get()->rememberLevel(lvl);
    }
    auto indexOf = [this](int id) {
        auto it = std::find(m_pageIds.begin(), m_pageIds.end(), id);
        return static_cast<int>(it - m_pageIds.begin());
    };
    std::sort(sorted.begin(), sorted.end(), [&](GJGameLevel* a, GJGameLevel* b) {
        return indexOf(a->m_levelID.value()) < indexOf(b->m_levelID.value());
    });
    auto arr = CCArray::create();
    for (auto lvl : sorted) arr->addObject(lvl);

    m_shownLevels = arr;
    rebuildListView(false);
}

// ---------- фоны ----------

// Картинки gsl_bg_1.jpg, gsl_bg_2.jpg, ... из resources/backgrounds — сколько есть
void GSLListLayer::setupBackgrounds() {
    auto fu = CCFileUtils::sharedFileUtils();
    for (int i = 1; i <= 100; i++) {
        auto name = fmt::format("{}/gsl_bg_{}.jpg", Mod::get()->getID(), i);
        auto path = fu->fullPathForFilename(name.c_str(), true);
        if (!fu->isFileExist(path)) break;
        m_bgFiles.push_back(name);
    }
    if (m_bgFiles.empty()) return; // картинок нет — остаётся обычный синий фон

    m_bgIndex = static_cast<size_t>(std::rand()) % m_bgFiles.size();
    m_bgSprite = makeBackground(m_bgFiles[m_bgIndex]);
    if (m_bgSprite) this->addChild(m_bgSprite, -1);
    if (m_bgFiles.size() > 1) {
        auto interval = static_cast<float>(Mod::get()->getSettingValue<int64_t>("bg-interval"));
        this->schedule(schedule_selector(GSLListLayer::nextBackground), std::max(5.f, interval));
    }
}

// Размытие без шейдеров: картинку по шагам уменьшаем в 2 раза (с плавной
// фильтрацией), потом растягиваем обратно на весь экран — получается мягкий блюр
static CCSprite* blurSprite(CCSprite* src, int level) {
    auto win = CCDirector::get()->getWinSize();
    float target = 1.f + level * 1.5f; // во сколько раз уменьшить
    CCSprite* cur = src;
    CCSize size = win;
    float total = 1.f;
    while (total < target - .01f) {
        float step = std::min(2.f, target / total);
        size = CCSize(std::max(1.f, size.width / step), std::max(1.f, size.height / step));
        auto rt = CCRenderTexture::create(static_cast<int>(size.width), static_cast<int>(size.height));
        if (!rt) return src;
        // в CCRenderTexture начало координат — левый нижний угол текстуры
        cur->setScale(cur->getScale() / step);
        cur->setPosition(size / 2);
        rt->begin();
        cur->visit();
        rt->end();
        // берём ровно нарисованную часть (текстура может быть больше, если степень двойки)
        auto rtSpr = rt->getSprite();
        auto next = CCSprite::createWithTexture(rtSpr->getTexture(), rtSpr->getTextureRect());
        next->setFlipY(true);
        next->getTexture()->setAntiAliasTexParameters();
        cur = next;
        total *= step;
    }
    // растягиваем обратно чуть больше экрана: края размытой картинки
    // смешиваются с пустотой, пусть они уходят за экран
    auto cs = cur->getContentSize();
    float cover = std::max(win.width / cs.width, win.height / cs.height);
    cur->setScale(cover * (1.f + .015f * level));
    cur->setPosition(win / 2);
    return cur;
}

// Картинка на весь экран (обрезается по краям, без растягивания);
// размытие и затемнение — из настроек
CCSprite* GSLListLayer::makeBackground(std::string const& file) {
    auto spr = CCSprite::create(file.c_str());
    if (!spr) return nullptr;
    auto win = CCDirector::get()->getWinSize();
    auto size = spr->getContentSize();
    spr->setScale(std::max(win.width / size.width, win.height / size.height));
    spr->setPosition(win / 2);
    spr->getTexture()->setAntiAliasTexParameters();

    auto blur = static_cast<int>(Mod::get()->getSettingValue<int64_t>("bg-blur"));
    if (blur > 0) spr = blurSprite(spr, std::clamp(blur, 0, 10));

    auto darken = std::clamp(static_cast<int>(Mod::get()->getSettingValue<int64_t>("bg-darken")), 0, 90);
    auto c = static_cast<GLubyte>(255 * (100 - darken) / 100);
    spr->setColor({ c, c, c });
    return spr;
}

void GSLListLayer::nextBackground(float) {
    if (m_bgFiles.size() < 2) return;
    m_bgIndex = (m_bgIndex + 1) % m_bgFiles.size();
    auto next = makeBackground(m_bgFiles[m_bgIndex]);
    if (!next) return;

    // новая картинка проявляется поверх старой, старая потом убирается
    float t = 1.5f;
    next->setOpacity(0);
    this->addChild(next, -1);
    next->runAction(CCFadeIn::create(t));
    if (auto old = m_bgSprite) {
        old->runAction(CCSequence::create(CCDelayTime::create(t), CCRemoveSelf::create(), nullptr));
    }
    m_bgSprite = next;
}

// ---------- прячущийся поиск ----------

std::string GSLListLayer::searchMode() const {
    return Mod::get()->getSettingValue<std::string>("search-bar-mode");
}

// Список под поиском: когда поиск спрятан, список выше на высоту полосы.
// keepScroll — не сдвигать уровни на экране при смене высоты.
void GSLListLayer::rebuildListView(bool keepScroll) {
    if (!m_shownLevels) return;
    float oldY = 0.f;
    bool hadOld = false;
    if (auto old = m_listLayer->m_listView) {
        if (old->m_tableView && old->m_tableView->m_contentLayer) {
            oldY = old->m_tableView->m_contentLayer->getPositionY();
            hadOld = true;
        }
        old->removeFromParent();
        old->release();
    }
    float h = LIST_H + (m_searchHidden ? SEARCH_H : 0.f);
    auto listView = CustomListView::create(m_shownLevels, BoomListType::Level, h, LIST_W);
    listView->retain();
    m_listLayer->addChild(listView, 6, 9);
    m_listLayer->m_listView = listView;

    auto content = listView->m_tableView ? listView->m_tableView->m_contentLayer : nullptr;
    if (content && keepScroll && hadOld) {
        // низ списка на месте; не даём уехать выше начала или ниже конца
        float minY = h - content->getContentHeight();
        content->setPositionY(std::clamp(oldY, std::min(minY, 0.f), std::max(minY, 0.f)));
    }
    m_lastScrollY = content ? content->getPositionY() : 0.f;
    m_scrollAccum = 0.f;
}

void GSLListLayer::setSearchHidden(bool hidden) {
    if (hidden == m_searchHidden || m_searchAnimating || !m_searchStrip) return;
    m_searchAnimating = true;
    m_searchHidden = hidden;
    float t = .22f;

    if (m_searchToggle) {
        if (auto arrow = m_searchToggle->getNormalImage()->getChildByTag(1)) {
            arrow->stopAllActions();
            // спрятан — стрелка вниз (открыть), открыт — вверх (закрыть)
            arrow->runAction(CCEaseInOut::create(CCRotateTo::create(t, hidden ? -90.f : 90.f), 2.f));
        }
    }

    Ref<GSLListLayer> self = this;
    m_searchStrip->stopAllActions();
    if (hidden) {
        // полоса плавно сжимается к заголовку, потом список занимает её место
        m_searchStrip->runAction(CCSequence::create(
            CCEaseInOut::create(CCScaleTo::create(t, 1.f, 0.f), 2.f),
            CallFuncExt::create([self] {
                self->m_searchStrip->setVisible(false);
                self->rebuildListView(true);
                self->m_searchAnimating = false;
            }),
            nullptr
        ));
    }
    else {
        // список сразу уступает место, полоса плавно выезжает
        rebuildListView(true);
        m_searchStrip->setVisible(true);
        m_searchStrip->setScaleY(0.f);
        m_searchStrip->runAction(CCSequence::create(
            CCEaseInOut::create(CCScaleTo::create(t, 1.f, 1.f), 2.f),
            CallFuncExt::create([self] { self->m_searchAnimating = false; }),
            nullptr
        ));
    }
}

void GSLListLayer::onToggleSearch(CCObject*) {
    setSearchHidden(!m_searchHidden);
}

// Следим за прокруткой: прячем поиск, только если пролистали вниз заметно
// (мелкие движения и дрожание пальца не считаются)
void GSLListLayer::update(float) {
    if (m_listLayer) styleCells(m_listLayer->m_listView);
    if (m_searchAnimating || m_searchHidden || searchMode() != "Hide on scroll") return;
    auto listView = m_listLayer ? m_listLayer->m_listView : nullptr;
    if (!listView || !listView->m_tableView || !listView->m_tableView->m_contentLayer) return;

    float y = listView->m_tableView->m_contentLayer->getPositionY();
    float delta = y - m_lastScrollY; // > 0 — листаем вниз по списку
    m_lastScrollY = y;
    m_scrollAccum = std::max(0.f, m_scrollAccum + delta);
    if (m_scrollAccum > 45.f) setSearchHidden(true);
}

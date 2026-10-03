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

// Маленькое окно с полем для числа (Custom Top N)
class GSLNumberPopup : public Popup {
public:
    static GSLNumberPopup* create(char const* title, int value, std::function<void(int)> done) {
        auto ret = new GSLNumberPopup();
        ret->m_done = std::move(done);
        if (ret->init(title, value)) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
protected:
    std::function<void(int)> m_done;
    TextInput* m_input = nullptr;
    bool init(char const* title, int value) {
        if (!Popup::init(220.f, 130.f)) return false;
        this->setTitle(title);
        m_input = TextInput::create(120.f, "Number", "bigFont.fnt");
        m_input->setCommonFilter(CommonFilter::Uint);
        if (value > 0) m_input->setString(std::to_string(value));
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, { 0.f, 5.f });
        auto spr = ButtonSprite::create("OK", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLNumberPopup::onOK));
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, { 0.f, 25.f });
        return true;
    }
    void onOK(CCObject*) {
        int v = std::atoi(m_input->getString().c_str());
        if (v > 0 && m_done) m_done(v);
        this->onClose(nullptr);
    }
};

// Выбор любых длин (Custom length): Tiny ... XL + Platformer
class GSLLengthPopup : public Popup {
public:
    static GSLLengthPopup* create(int mask, std::function<void(int)> done) {
        auto ret = new GSLLengthPopup();
        ret->m_mask = mask;
        ret->m_done = std::move(done);
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
protected:
    int m_mask = 0;
    std::function<void(int)> m_done;
    bool init() {
        if (!Popup::init(240.f, 200.f)) return false;
        this->setTitle("Custom length");
        char const* names[] = { "Tiny", "Short", "Medium", "Long", "XL", "Platformer" };
        auto menu = CCMenu::create();
        menu->setContentSize({ 200.f, 120.f });
        menu->ignoreAnchorPointForPosition(false);
        for (int i = 0; i < 6; i++) {
            float x = (i % 2) * 100.f + 15.f;
            float y = 105.f - (i / 2) * 40.f;
            auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(GSLLengthPopup::onToggle), .6f);
            t->toggle(m_mask & (1 << i));
            t->setTag(i);
            t->setPosition({ x, y });
            menu->addChild(t);
            auto lbl = CCLabelBMFont::create(names[i], "bigFont.fnt");
            lbl->limitLabelWidth(70.f, .4f, .1f);
            lbl->setAnchorPoint({ 0.f, .5f });
            lbl->setPosition({ x + 18.f, y });
            menu->addChild(lbl);
        }
        m_mainLayer->addChildAtPosition(menu, Anchor::Center, { 0.f, 0.f });
        auto spr = ButtonSprite::create("OK", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLLengthPopup::onOK));
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, { 0.f, 25.f });
        return true;
    }
    void onToggle(CCObject* sender) { m_mask ^= 1 << sender->getTag(); }
    void onOK(CCObject*) {
        if (m_done) m_done(m_mask);
        this->onClose(nullptr);
    }
};

// Фильтры — как окно фильтров в Global Demonlist
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
    static constexpr int CUSTOM = -1;
    static constexpr int PRESET_LENGTHS = 0b011110; // Short..XL

    GSLListLayer* m_layer = nullptr;
    int m_rank = 0;
    int m_lengthMask = 0;
    bool m_completedBy = false, m_createdBy = false;
    std::vector<std::pair<int, CCLabelBMFont*>> m_rankLabels;
    std::vector<std::pair<int, CCLabelBMFont*>> m_lengthLabels;
    TextInput* m_victorInput = nullptr;
    TextInput* m_creatorInput = nullptr;

    // Ряд: иконка слева, подписи по ширине рамки (никогда не вылезают), шестерёнка справа
    void addRow(float y, CCSprite* icon, std::vector<std::pair<int, char const*>> const& items,
                SEL_MenuHandler onItem, SEL_MenuHandler onGear, std::vector<std::pair<int, CCLabelBMFont*>>& out) {
        float boxW = m_mainLayer->getContentWidth() - 30.f;
        float boxH = 46.f;
        auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(60);
        bg->setContentSize({ boxW, boxH });
        m_mainLayer->addChildAtPosition(bg, Anchor::Top, { 0.f, y });

        auto menu = CCMenu::create();
        menu->setContentSize({ boxW, boxH });
        menu->ignoreAnchorPointForPosition(false);
        m_mainLayer->addChildAtPosition(menu, Anchor::Top, { 0.f, y });

        float left = 10.f;
        if (icon) {
            icon->setScale(30.f / std::max(icon->getContentWidth(), icon->getContentHeight()));
            icon->setPosition({ left + 15.f, boxH / 2 });
            menu->addChild(icon);
            left += 36.f;
        }
        float right = boxW - 8.f;
        auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
        if (!gearSpr) gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        if (gearSpr) {
            gearSpr->setScale(28.f / gearSpr->getContentWidth());
            auto gear = CCMenuItemSpriteExtra::create(gearSpr, this, onGear);
            gear->setPosition({ right - 14.f, boxH / 2 });
            menu->addChild(gear);
            right -= 34.f;
        }

        // подбираем один масштаб для всех подписей, чтобы они влезли с отступами
        std::vector<CCLabelBMFont*> labels;
        float textW = 0.f;
        for (auto& [val, name] : items) {
            auto lbl = CCLabelBMFont::create(name, "bigFont.fnt");
            labels.push_back(lbl);
            textW += lbl->getContentWidth();
        }
        // одинаковые промежутки между подписями; если тесно — уменьшаем шрифт
        float minGap = 8.f;
        float n = static_cast<float>(items.size());
        float scale = std::min(.45f, (right - left - minGap * (n + 1)) / textW);
        float gap = (right - left - textW * scale) / (n + 1);
        float x = left + gap;
        for (size_t i = 0; i < items.size(); i++) {
            auto lbl = labels[i];
            lbl->setScale(scale);
            auto btn = CCMenuItemSpriteExtra::create(lbl, this, onItem);
            btn->setTag(items[i].first);
            btn->setPosition({ x + lbl->getScaledContentWidth() / 2, boxH / 2 });
            menu->addChild(btn);
            out.push_back({ items[i].first, lbl });
            x += lbl->getScaledContentWidth() + gap;
        }
    }

    void addCheck(CCMenu* menu, float x, float y, char const* text, bool on, SEL_MenuHandler cb,
                  char const* info, CCMenuItemToggler** out = nullptr) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, .65f);
        t->toggle(on);
        t->setPosition({ x, y });
        menu->addChild(t);
        if (out) *out = t;
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->limitLabelWidth(80.f, .45f, .1f);
        lbl->setAnchorPoint({ 0.f, .5f });
        lbl->setPosition({ x + 18.f, y - 1.f });
        menu->addChild(lbl);

        auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        infoSpr->setScale(.45f);
        auto infoBtn = CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(GSLFilterPopup::onInfo));
        infoBtn->setUserObject("info", CCString::create(info));
        infoBtn->setPosition({ x + 18.f + lbl->getScaledContentWidth() + 5.f, y + 9.f });
        menu->addChild(infoBtn);
    }

    bool init() {
        if (!Popup::init(420.f, 270.f)) return false;
        this->setTitle("Search Filters", "goldFont.fnt", .9f);
        m_rank = m_layer->m_rankLimit;
        m_lengthMask = m_layer->m_lengthMask;
        m_completedBy = m_layer->m_completedBy;
        m_createdBy = m_layer->m_onlyCreator;

        // Ряд 1: места в листе
        addRow(-55.f, CCSprite::createWithSpriteFrameName("diffIcon_10_btn_001.png"),
            { { 0, "Unbounded" }, { 75, "Top 75" }, { 150, "Top 150" }, { 300, "Top 300" }, { CUSTOM, "Custom" } },
            menu_selector(GSLFilterPopup::onRank), menu_selector(GSLFilterPopup::onRankGear), m_rankLabels);

        // Ряд 2: длина
        addRow(-109.f, CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png"),
            { { 1 << 1, "Short" }, { 1 << 2, "Medium" }, { 1 << 3, "Long" }, { 1 << 4, "XL" }, { CUSTOM, "Custom" } },
            menu_selector(GSLFilterPopup::onLength), menu_selector(GSLFilterPopup::onLengthGear), m_lengthLabels);

        // Ряд 3: галочки и поля
        float boxW = m_mainLayer->getContentWidth() - 30.f;
        float boxH = 74.f;
        auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(60);
        bg->setContentSize({ boxW, boxH });
        m_mainLayer->addChildAtPosition(bg, Anchor::Top, { 0.f, -180.f });

        auto checks = CCMenu::create();
        checks->ignoreAnchorPointForPosition(false);
        checks->setContentSize({ boxW, boxH });
        m_mainLayer->addChildAtPosition(checks, Anchor::Top, { 0.f, -180.f });
        float rowTop = boxH - 20.f, rowBottom = 20.f;
        addCheck(checks, 40.f, rowTop, "Completed By", m_completedBy, menu_selector(GSLFilterPopup::onCompleted),
            "Only levels this player has beaten (victors on the site).\nEmpty name = <cg>your</c> account.");
        addCheck(checks, 40.f, rowBottom, "Created By", m_createdBy, menu_selector(GSLFilterPopup::onCreated),
            "Only levels made by this creator.");

        float inputW = 150.f;
        float inputX = boxW / 2 - inputW / 2 - 30.f;
        m_victorInput = TextInput::create(inputW, "Username", "bigFont.fnt");
        m_victorInput->setString(m_layer->m_victor);
        m_mainLayer->addChildAtPosition(m_victorInput, Anchor::Top, { inputX, -180.f + boxH / 2 - 20.f });
        m_creatorInput = TextInput::create(inputW, "Creator Name", "bigFont.fnt");
        m_creatorInput->setString(m_layer->m_creator);
        m_mainLayer->addChildAtPosition(m_creatorInput, Anchor::Top, { inputX, -180.f - boxH / 2 + 20.f });

        // Apply
        auto spr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", .9f);
        auto apply = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLFilterPopup::onApply));
        m_buttonMenu->addChildAtPosition(apply, Anchor::Bottom, { 0.f, 26.f });

        updateLabels();
        return true;
    }

    static bool isPresetRank(int r) { return r == 0 || r == 75 || r == 150 || r == 300; }

    void updateLabels() {
        ccColor3B on = { 255, 255, 255 }, off = { 120, 120, 120 };
        for (auto& [val, lbl] : m_rankLabels) {
            bool sel = val == CUSTOM ? !isPresetRank(m_rank) : val == m_rank;
            lbl->setColor(sel ? on : off);
        }
        for (auto& [val, lbl] : m_lengthLabels) {
            bool sel = val == CUSTOM ? (m_lengthMask & ~PRESET_LENGTHS) != 0 : (m_lengthMask & val) != 0;
            lbl->setColor(sel ? on : off);
        }
    }

    void openCustomRank() {
        Ref<GSLFilterPopup> self = this;
        GSLNumberPopup::create("Custom top", isPresetRank(m_rank) ? 0 : m_rank, [self](int v) {
            self->m_rank = v;
            self->updateLabels();
        })->show();
    }
    void openCustomLength() {
        Ref<GSLFilterPopup> self = this;
        GSLLengthPopup::create(m_lengthMask, [self](int mask) {
            self->m_lengthMask = mask;
            self->updateLabels();
        })->show();
    }

    void onRank(CCObject* sender) {
        int v = sender->getTag();
        if (v == CUSTOM) return openCustomRank();
        m_rank = v;
        updateLabels();
    }
    void onRankGear(CCObject*) { openCustomRank(); }
    void onLength(CCObject* sender) {
        int v = sender->getTag();
        if (v == CUSTOM) return openCustomLength();
        m_lengthMask ^= v;
        updateLabels();
    }
    void onLengthGear(CCObject*) { openCustomLength(); }

    void onCompleted(CCObject*) { m_completedBy = !m_completedBy; }
    void onCreated(CCObject*) { m_createdBy = !m_createdBy; }

    void onInfo(CCObject* sender) {
        auto text = static_cast<CCString*>(static_cast<CCNode*>(sender)->getUserObject("info"));
        FLAlertLayer::create("Info", text ? text->getCString() : "", "OK")->show();
    }

    void onApply(CCObject*) {
        m_layer->m_rankLimit = m_rank;
        m_layer->m_lengthMask = m_lengthMask;
        m_layer->m_completedBy = m_completedBy;
        m_layer->m_victor = m_victorInput->getString();
        m_layer->m_onlyCreator = m_createdBy;
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

    // Стрелочка к «i» на Future Classic — пока пояснение не прочитано
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

    updateListButton();
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

    if (auto icon = CCSprite::createWithSpriteFrameName("rankIcon_1_001.png")) {
        icon->setScale(20.f / icon->getContentHeight());
        icon->setPosition({ size / 2, size - pad - 10.f });
        bg->addChild(icon);
    }

    // слова названия — каждое отдельной строкой, все одним масштабом
    auto words = utils::string::split(GSLManager::prettyListName(m_list), " ");
    float textH = 16.f; // место под текст снизу
    float scale = .5f;
    std::vector<CCLabelBMFont*> lines;
    for (auto& w : words) {
        if (w.empty()) continue;
        auto lbl = CCLabelBMFont::create(w.c_str(), "bigFont.fnt");
        scale = std::min(scale, inner / lbl->getContentWidth());
        lines.push_back(lbl);
    }
    if (!lines.empty()) {
        float lineH = lines[0]->getContentHeight() * .8f;
        scale = std::min(scale, textH / (lineH * lines.size()));
        float y = pad + 1.f + lineH * scale * (lines.size() - .5f);
        for (auto lbl : lines) {
            lbl->setScale(scale);
            lbl->setPosition({ size / 2, y });
            bg->addChild(lbl);
            y -= lineH * scale;
        }
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
        text += "\n\n<co>FUTURE CLASSIC:</c> only levels that already have a "
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
    bool active = m_rankLimit > 0 || m_lengthMask != 0 || m_completedBy
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
        if (m_rankLimit > 0 && e.rank > m_rankLimit) continue;
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

    if (m_listLayer) m_listLayer->removeFromParent();

    auto win = CCDirector::get()->getWinSize();
    auto listView = CustomListView::create(arr, BoomListType::Level, LIST_H, LIST_W);
    m_listLayer = GJListLayer::create(
        listView, GSLManager::prettyListName(m_list).c_str(), { 191, 114, 62, 255 }, LIST_W, LIST_H, 0
    );
    m_listLayer->setPosition({ win.width / 2 - LIST_W / 2, listBottomY() });
    this->addChild(m_listLayer, 1);
}

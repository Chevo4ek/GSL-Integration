#include "GSLPopup.hpp"
#include "GSLManager.hpp"

static std::vector<std::string> const LISTS = {
    "classic", "challenge", "spam", "future", "future_challenge", "future_spam", "impossible"
};

GSLPopup* GSLPopup::create() {
    auto ret = new GSLPopup();
    if (ret->initAnchored(400.f, 270.f)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool GSLPopup::setup() {
    this->setTitle("Global Shitty List");
    auto size = m_mainLayer->getContentSize();

    // Вкладки
    m_tabs = CCMenu::create();
    m_tabs->setContentSize({ size.width - 20.f, 30.f });
    m_tabs->setLayout(RowLayout::create()->setGap(4.f)->setAutoScale(true));
    for (size_t i = 0; i < LISTS.size(); i++) {
        auto spr = ButtonSprite::create(
            GSLManager::prettyListName(LISTS[i]).c_str(), "bigFont.fnt", "GJ_button_04.png", .6f
        );
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLPopup::onTab));
        btn->setTag(static_cast<int>(i));
        m_tabs->addChild(btn);
    }
    m_tabs->updateLayout();
    m_mainLayer->addChildAtPosition(m_tabs, Anchor::Top, { 0.f, -48.f });

    // Список
    m_scroll = ScrollLayer::create({ size.width - 30.f, size.height - 100.f });
    m_scroll->m_contentLayer->setLayout(
        ColumnLayout::create()->setAxisReverse(true)->setAxisAlignment(AxisAlignment::End)
            ->setAutoGrowAxis(m_scroll->getContentHeight())->setGap(2.f)
    );
    m_mainLayer->addChildAtPosition(m_scroll, Anchor::Bottom,
        { -m_scroll->getContentWidth() / 2, 18.f });

    m_status = CCLabelBMFont::create("", "goldFont.fnt");
    m_status->setScale(.4f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, { 0.f, 9.f });

    // Кнопка "обновить сейчас"
    auto refreshSpr = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png");
    refreshSpr->setScale(.6f);
    auto refreshBtn = CCMenuItemSpriteExtra::create(refreshSpr, this, menu_selector(GSLPopup::onRefresh));
    m_buttonMenu->addChildAtPosition(refreshBtn, Anchor::TopRight, { -20.f, -20.f });

    selectList(m_current);
    return true;
}

void GSLPopup::onTab(CCObject* sender) {
    selectList(LISTS[static_cast<size_t>(sender->getTag())]);
}

void GSLPopup::onRefresh(CCObject*) {
    GSLManager::get()->forceRefresh();
    Notification::create("Refreshing list...", NotificationIcon::Loading)->show();
}

void GSLPopup::selectList(std::string const& list) {
    m_current = list;
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    auto entries = GSLManager::get()->getList(list);
    float w = m_scroll->getContentWidth();

    for (auto& e : entries) {
        auto row = CCMenu::create();
        row->setContentSize({ w, 26.f });
        row->ignoreAnchorPointForPosition(false);

        auto bg = CCLayerColor::create({ 0, 0, 0, 70 }, w, 26.f);
        bg->setZOrder(-1);
        row->addChild(bg);

        auto rank = CCLabelBMFont::create(fmt::format("#{}", e.rank).c_str(), "goldFont.fnt");
        rank->setScale(.5f);
        rank->setAnchorPoint({ 0.f, .5f });
        rank->setPosition({ 6.f, 13.f });
        row->addChild(rank);

        auto name = CCLabelBMFont::create(e.name.c_str(), "bigFont.fnt");
        name->setAnchorPoint({ 0.f, .5f });
        name->setPosition({ 50.f, 17.f });
        name->limitLabelWidth(w - 130.f, .4f, .1f);
        row->addChild(name);

        auto author = CCLabelBMFont::create(fmt::format("by {}", e.author).c_str(), "chatFont.fnt");
        author->setAnchorPoint({ 0.f, .5f });
        author->setPosition({ 50.f, 6.f });
        author->limitLabelWidth(w - 130.f, .45f, .1f);
        author->setColor({ 200, 200, 200 });
        row->addChild(author);

        auto viewSpr = ButtonSprite::create("View", "bigFont.fnt", "GJ_button_01.png", .5f);
        viewSpr->setScale(.6f);
        auto view = CCMenuItemSpriteExtra::create(viewSpr, this, menu_selector(GSLPopup::onLevel));
        view->setTag(e.levelId);
        view->setPosition({ w - 30.f, 13.f });
        row->addChild(view);

        content->addChild(row);
    }
    content->updateLayout();
    m_scroll->scrollToTop();

    auto mgr = GSLManager::get();
    m_status->setString(
        entries.empty()
            ? (mgr->isLoading() ? "Loading..." : "No levels")
            : fmt::format("{} levels", entries.size()).c_str()
    );
}

void GSLPopup::onLevel(CCObject* sender) {
    auto search = GJSearchObject::create(SearchType::Search, std::to_string(sender->getTag()));
    auto scene = LevelBrowserLayer::scene(search);
    CCDirector::get()->pushScene(CCTransitionFade::create(.5f, scene));
}

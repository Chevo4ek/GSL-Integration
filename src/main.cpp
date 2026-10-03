#include <Geode/Geode.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include "GSLManager.hpp"
#include "GSLPopup.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    GSLManager::get()->start();
}

// Значки "#12 Classic" сбоку у каждого уровня в результатах поиска
class $modify(GSLLevelCell, LevelCell) {
    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        if (!level) return;

        auto entries = GSLManager::get()->find(level->m_levelID.value());
        if (!entries || entries->empty()) return;

        auto box = CCNode::create();
        box->setID("gsl-badges"_spr);
        box->setAnchorPoint({ 1.f, 1.f });
        box->setContentSize({ 90.f, 40.f });
        box->setLayout(
            ColumnLayout::create()->setAxisReverse(true)->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisLineAlignment(AxisAlignment::End)->setGap(1.f)
        );

        for (auto& e : *entries) {
            auto label = CCLabelBMFont::create(
                fmt::format("#{} {}", e.rank, GSLManager::prettyListName(e.list)).c_str(),
                "bigFont.fnt"
            );
            label->setScale(.3f);
            label->setColor(GSLManager::listColor(e.list));
            box->addChild(label);
        }
        box->updateLayout();

        // Правый верхний угол клетки, над кнопкой View
        float w = m_compactView ? 320.f : 356.f;
        float h = m_compactView ? 50.f : 90.f;
        box->setPosition({ w - 8.f, h - 4.f });
        this->addChild(box, 10);
    }
};

// Кнопка листа в окне поиска уровней
class $modify(GSLSearchLayer, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;

        auto spr = ButtonSprite::create("GSL", "goldFont.fnt", "GJ_button_02.png", .8f);
        spr->setScale(.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLSearchLayer::onGSL));
        btn->setID("gsl-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("gsl-menu"_spr);
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({ win.width - 35.f, 35.f });
        this->addChild(menu, 10);
        return true;
    }

    void onGSL(CCObject*) {
        GSLPopup::create()->show();
    }
};

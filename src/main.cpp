#include <Geode/Geode.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "GSLManager.hpp"
#include "GSLPopup.hpp"

using namespace geode::prelude;

// Запускаем синхронизацию, когда открылось главное меню
class $modify(GSLMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        GSLManager::get()->start(); // повторный вызов ничего не делает
        return true;
    }
};

static GJDifficultySprite* findDifficulty(CCNode* node) {
    if (!node || !node->getChildren()) return nullptr;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto d = typeinfo_cast<GJDifficultySprite*>(child)) return d;
        if (auto d = findDifficulty(child)) return d;
    }
    return nullptr;
}

// difficulty из API -> кадр мордашки GD и подпись
static std::pair<int, char const*> demonFrame(std::string const& d) {
    if (d == "easy_demon") return { 7, "Easy Demon" };
    if (d == "medium_demon") return { 8, "Medium Demon" };
    if (d == "hard_demon") return { 6, "Hard Demon" };
    if (d == "insane_demon") return { 9, "Insane Demon" };
    if (d == "extreme_demon") return { 10, "Extreme Demon" };
    return { -1, nullptr };
}

static bool isDifficultyText(std::string const& s) {
    static std::vector<std::string> const names = {
        "Auto", "Easy", "Normal", "Hard", "Harder", "Insane", "N/A", "NA", "Unrated"
    };
    if (s.find("Demon") != std::string::npos) return true;
    return std::find(names.begin(), names.end(), s) != names.end();
}

// Место в листе под мордашкой сложности + замена мордашки на демона из листа
class $modify(GSLLevelCell, LevelCell) {
    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        if (!level) return;

        auto entries = GSLManager::get()->find(level->m_levelID.value());
        if (!entries || entries->empty()) return;

        auto diff = findDifficulty(m_mainLayer);
        if (!diff) return;
        auto parent = diff->getParent();

        // Ищем подпись сложности ("Insane") рядом с мордашкой
        CCLabelBMFont* diffLabel = nullptr;
        for (auto child : CCArrayExt<CCNode*>(parent->getChildren())) {
            if (auto lbl = typeinfo_cast<CCLabelBMFont*>(child)) {
                if (lbl->getString() && isDifficultyText(lbl->getString())) { diffLabel = lbl; break; }
            }
        }

        // Мордашка и подпись — как на сайте
        auto [frame, text] = demonFrame(entries->front().difficulty);
        if (frame > 0) {
            diff->updateDifficultyFrame(frame, GJDifficultyName::Short);
            if (diffLabel) {
                float sc = diffLabel->getScale();
                diffLabel->setString(text);
                diffLabel->limitLabelWidth(70.f, sc, .1f);
            }
        }

        // Позиции в листах, по строке на список
        float y = (diffLabel ? diffLabel->getPositionY() : diff->getPositionY() - 22.f) - 11.f;
        for (auto& e : *entries) {
            auto label = CCLabelBMFont::create(
                fmt::format("#{} {}", e.rank, GSLManager::prettyListName(e.list)).c_str(), "goldFont.fnt"
            );
            label->limitLabelWidth(70.f, .4f, .1f);
            label->setColor(GSLManager::listColor(e.list));
            label->setPosition({ diff->getPositionX(), y });
            label->setID("gsl-rank"_spr);
            parent->addChild(label, 10);
            y -= 10.f;
        }
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
        CCDirector::get()->pushScene(CCTransitionFade::create(.5f, GSLListLayer::scene()));
    }
};

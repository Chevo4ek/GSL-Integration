#include <Geode/Geode.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
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
static std::pair<int, char const*> demonFrame(std::string d) {
    for (auto& c : d) c = (c == ' ' || c == '-') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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

// Границы узла в координатах экрана
static CCRect worldBox(CCNode* n) {
    auto bb = n->boundingBox();
    auto p = n->getParent();
    if (!p) return bb;
    auto a = p->convertToWorldSpace({ bb.getMinX(), bb.getMinY() });
    auto b = p->convertToWorldSpace({ bb.getMaxX(), bb.getMaxY() });
    return { a.x, a.y, b.x - a.x, b.y - a.y };
}

static void collect(CCNode* node, std::vector<CCNode*>& out) {
    if (!node || !node->getChildren()) return;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        out.push_back(child);
        collect(child, out);
    }
}

// Общая логика для карточки уровня и страницы уровня:
// мордашка демона из листа + подпись сложности + место(а) в листе
// под мордашкой (или под монетками, если они есть).
static void applyGSL(CCNode* root, GJDifficultySprite* diff, std::vector<GSLEntry> const& all, bool fullNames) {
    auto parent = diff->getParent();
    if (!parent) return;

    // убираем наши старые подписи (если вызвали повторно)
    while (auto old = parent->getChildByID("gsl-rank"_spr)) old->removeFromParent();

    // Future Classic — без мест и без смены сложности
    std::vector<GSLEntry> entries;
    for (auto& e : all) if (e.list != "future") entries.push_back(e);
    if (entries.empty()) return;

    std::vector<CCNode*> nodes;
    collect(root, nodes);

    auto diffBox = worldBox(diff);
    float cx = diffBox.getMidX();

    // подпись сложности рядом с мордашкой
    CCLabelBMFont* diffLabel = nullptr;
    float best = 1e9f;
    for (auto n : nodes) {
        auto lbl = typeinfo_cast<CCLabelBMFont*>(n);
        if (!lbl || !lbl->getString() || !isDifficultyText(lbl->getString())) continue;
        auto box = worldBox(lbl);
        float d = std::abs(box.getMidX() - cx) + std::abs(box.getMaxY() - diffBox.getMinY());
        if (d < best && d < 80.f) { best = d; diffLabel = lbl; }
    }

    auto [frame, text] = demonFrame(entries.front().difficulty);
    if (frame > 0) {
        diff->updateDifficultyFrame(frame, GJDifficultyName::Short);
        if (diffLabel) {
            float sc = diffLabel->getScale();
            diffLabel->setString(text);
            diffLabel->limitLabelWidth(fullNames ? 90.f : 60.f, sc, .1f);
        }
    }

    // самая нижняя точка столбика "мордашка / подпись / монетки"
    float bottom = diffBox.getMinY();
    if (diffLabel) bottom = std::min(bottom, worldBox(diffLabel).getMinY());
    float range = fullNames ? 55.f : 45.f;
    for (auto n : nodes) {
        if (n == diff || !n->isVisible() || !typeinfo_cast<CCSprite*>(n)) continue;
        if (typeinfo_cast<GJDifficultySprite*>(n->getParent())) continue;
        auto box = worldBox(n);
        if (box.size.width > 22.f || box.size.height > 22.f) continue; // монетки маленькие
        if (std::abs(box.getMidX() - cx) > 30.f) continue;
        if (box.getMaxY() > diffBox.getMinY() + 2.f) continue;
        if (box.getMaxY() < diffBox.getMinY() - range) continue;
        bottom = std::min(bottom, box.getMinY());
    }

    float lineH = fullNames ? 14.f : 9.f;
    float y = bottom - (fullNames ? 9.f : 6.f);
    for (auto& e : entries) {
        auto str = fullNames
            ? fmt::format("#{} {}", e.rank, GSLManager::prettyListName(e.list))
            : fmt::format("#{} {}", e.rank, GSLManager::shortListName(e.list));
        auto label = CCLabelBMFont::create(str.c_str(), "goldFont.fnt");
        if (fullNames) label->limitLabelWidth(110.f, .55f, .1f);
        else label->limitLabelWidth(40.f, .38f, .1f);
        label->setColor(GSLManager::listColor(e.list));
        label->setPosition(parent->convertToNodeSpace({ cx, y }));
        label->setID("gsl-rank"_spr);
        parent->addChild(label, 10);
        y -= lineH;
    }
}

// Карточка уровня в поиске
class $modify(GSLLevelCell, LevelCell) {
    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        if (!level) return;
        auto entries = GSLManager::get()->find(level->m_levelID.value());
        if (!entries || entries->empty()) return;
        Ref<LevelCell> self = this;
        auto list = *entries;
        Loader::get()->queueInMainThread([self, list] {
            if (auto diff = findDifficulty(self->m_mainLayer)) applyGSL(self, diff, list, false);
        });
    }
};

// Страница уровня (где кнопка Play)
class $modify(GSLLevelInfo, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        if (!level) return true;
        auto entries = GSLManager::get()->find(level->m_levelID.value());
        if (!entries || entries->empty()) return true;
        Ref<LevelInfoLayer> self = this;
        auto list = *entries;
        Loader::get()->queueInMainThread([self, list] {
            if (auto diff = findDifficulty(self)) applyGSL(self, diff, list, true);
        });
        return true;
    }
};

// Кнопка листа в окне поиска уровней
class $modify(GSLSearchLayer, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;

        // круглая кнопка с логотипом GSL
        auto spr = CCSprite::create("gsl_btn.png"_spr);
        spr->setScale(42.f / spr->getContentWidth());
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GSLSearchLayer::onGSL));
        btn->setID("gsl-button"_spr);

        // в правой колонке круглых кнопок, под глобусом
        auto menu = CCMenu::create();
        menu->setID("gsl-menu"_spr);
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({ win.width - 24.f, 53.f });
        this->addChild(menu, 10);
        return true;
    }

    void onGSL(CCObject*) {
        CCDirector::get()->pushScene(CCTransitionFade::create(.5f, GSLListLayer::scene()));
    }
};

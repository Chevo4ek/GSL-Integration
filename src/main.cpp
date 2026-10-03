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

        // кнопка GSL спрятана — напоминаем один раз за запуск игры
        static bool reminded = false;
        auto mod = Mod::get();
        if (!reminded && !mod->getSettingValue<bool>("show-gsl-button")
            && mod->getSettingValue<bool>("hidden-button-reminder")) {
            reminded = true;
            Loader::get()->queueInMainThread([] {
                Notification::create(
                    "GSL button is hidden - turn it on in the mod settings to open the list",
                    NotificationIcon::Warning, 4.f
                )->show();
            });
        }
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

// rate из листа ("featured", "epic", ...) -> рамка мордашки как в GD
static std::string lowerStr(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::optional<GJFeatureState> rateState(std::string const& rate) {
    auto r = lowerStr(rate);
    if (r == "featured") return GJFeatureState::Featured;
    if (r == "epic") return GJFeatureState::Epic;
    if (r == "legendary") return GJFeatureState::Legendary;
    if (r == "mythic") return GJFeatureState::Mythic;
    if (r == "none" || r == "rated" || r == "unrated" || r == "star" || r == "stars") return GJFeatureState::None;
    return std::nullopt; // неизвестное значение — не трогаем
}

static std::pair<std::string, ccColor3B> rateText(std::string const& rate) {
    auto r = lowerStr(rate);
    std::string text = rate;
    if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    if (r == "featured") return { text, { 255, 220, 80 } };
    if (r == "epic") return { text, { 255, 120, 60 } };
    if (r == "legendary") return { text, { 200, 120, 255 } };
    if (r == "mythic") return { text, { 120, 230, 255 } };
    return { text, { 255, 255, 255 } };
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
    if (frame > 0 && Mod::get()->getSettingValue<bool>("replace-face")) {
        diff->updateDifficultyFrame(frame, GJDifficultyName::Short);
        if (diffLabel) {
            while (auto old = diffLabel->getParent()->getChildByID("gsl-diff"_spr)) old->removeFromParent();
            auto own = CCLabelBMFont::create(text, diffLabel->getFntFile());
            own->setPosition(diffLabel->getPosition());
            own->setAnchorPoint(diffLabel->getAnchorPoint());
            own->limitLabelWidth(fullNames ? 90.f : 60.f, diffLabel->getScale(), .1f);
            own->setID("gsl-diff"_spr);
            diffLabel->getParent()->addChild(own, diffLabel->getZOrder());
            diffLabel->setVisible(false);
        }
    }

    // рамка Featured/Epic/... из rate листа
    bool showRate = Mod::get()->getSettingValue<bool>("show-rate");
    if (showRate) {
        if (auto st = rateState(entries.front().rate)) diff->updateFeatureState(*st);
    }

    // самая нижняя точка столбика "мордашка / подпись / монетки"
    float bottom = diffBox.getMinY();
    if (diffLabel) bottom = std::min(bottom, worldBox(diffLabel).getMinY());
    float range = fullNames ? 45.f : 40.f;
    for (auto n : nodes) {
        if (n == diff || !n->isVisible() || !typeinfo_cast<CCSprite*>(n)) continue;
        std::string nodeId = n->getID().data();
        if (nodeId.find("coin") == std::string::npos) continue; // только монетки
        if (typeinfo_cast<GJDifficultySprite*>(n->getParent())) continue;
        auto box = worldBox(n);
        if (box.size.width > 22.f || box.size.height > 22.f) continue; // монетки маленькие
        if (std::abs(box.getMidX() - cx) > 15.f) continue; // монетки строго под мордашкой
        if (box.getMaxY() > diffBox.getMinY() + 2.f) continue;
        if (box.getMaxY() < diffBox.getMinY() - range) continue;
        bottom = std::min(bottom, box.getMinY());
    }

    float lineH = fullNames ? 14.f : 9.f;
    float y = bottom - (fullNames ? 7.f : 6.f);
    for (auto& e : entries) {
        bool full = Mod::get()->getSettingValue<std::string>("rank-names") == "Full";
        auto str = full
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

    if (fullNames && showRate) {
        for (auto& e : entries) {
            if (e.rate.empty() || rateState(e.rate) == GJFeatureState::None) continue;
            auto [text, color] = rateText(e.rate);
            auto rate = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
            rate->limitLabelWidth(90.f, .4f, .1f);
            rate->setColor(color);
            rate->setPosition(parent->convertToNodeSpace({ cx, y }));
            rate->setID("gsl-rank"_spr); // удаляется вместе с подписями мест
            parent->addChild(rate, 10);
            break;
        }
    }
}

static bool isLengthText(std::string const& s) {
    static std::vector<std::string> const names = { "Tiny", "Short", "Medium", "Long", "XL", "Plat." };
    return std::find(names.begin(), names.end(), s) != names.end();
}

static bool insideMenuItem(CCNode* n) {
    for (auto p = n->getParent(); p; p = p->getParent()) {
        if (typeinfo_cast<CCMenuItem*>(p)) return true;
    }
    return false;
}

// Оформление карточки: очки листа, аккуратный ID,
// название трека не заходит на превью.
static void decorateCell(LevelCell* cell, std::vector<GSLEntry> const& all) {
    auto main = cell->m_mainLayer;
    if (!main) return;

    for (auto id : { "gsl-rank-badges", "gsl-points" }) {
        while (auto old = main->getChildByID(fmt::format("{}/{}", Mod::get()->getID(), id))) old->removeFromParent();
    }

    std::vector<GSLEntry> entries;
    for (auto& e : all) if (e.list != "future") entries.push_back(e);

    float w = cell->m_width > 0 ? cell->m_width : 356.f;
    float h = cell->m_height > 0 ? cell->m_height : 90.f;
    auto cellLeft = main->convertToWorldSpace({ 0.f, 0.f }).x;

    std::vector<CCNode*> nodes;
    collect(main, nodes);

    float rowY = -1.f, rowRight = 0.f;
    bool trimSong = Mod::get()->getSettingValue<bool>("trim-song-name");
    for (auto n : nodes) {
        auto lbl = typeinfo_cast<CCLabelBMFont*>(n);
        if (!lbl || !lbl->getString() || insideMenuItem(lbl)) continue;
        std::string str = lbl->getString();
        auto box = worldBox(lbl);
        auto c = lbl->getColor();

        // 1. название трека (розовая строка) — не дальше начала превью
        if (trimSong && c.r > 200 && c.b > 200 && c.g < 190 && str.find('#') != 0) {
            float maxW = (cellLeft + w * 0.60f) - box.getMinX();
            if (maxW > 20.f && box.size.width > maxW) {
                lbl->setScale(lbl->getScale() * maxW / box.size.width);
            }
        }

        // 4. ID уровня "#123..." — меньше и полупрозрачный (один раз)
        if (str.size() > 1 && str[0] == '#' && std::isdigit(static_cast<unsigned char>(str[1]))
            && !lbl->getUserObject("gsl-shrunk"_spr)) {
            lbl->setScale(lbl->getScale() * .7f);
            lbl->setOpacity(150);
            lbl->setUserObject("gsl-shrunk"_spr, CCBool::create(true));
        }

        if (isLengthText(str)) rowY = box.getMidY();
    }

    // 3. очки листа в строке статистики
    if (rowY > 0.f && !entries.empty()) {
        for (auto n : nodes) {
            if (!n->isVisible() || insideMenuItem(n)) continue;
            auto box = worldBox(n);
            if (box.size.width > 60.f) continue; // не фон и не превью
            if (std::abs(box.getMidY() - rowY) < 6.f) rowRight = std::max(rowRight, box.getMaxX());
        }
        auto pts = CCNode::create();
        pts->setID("gsl-points"_spr);
        auto orbs = CCSprite::createWithSpriteFrameName("currencyOrbIcon_001.png");
        float x = 0.f;
        if (orbs) {
            orbs->setScale(.55f);
            orbs->setPosition({ 0.f, 0.f });
            pts->addChild(orbs);
            x = 11.f;
        }
        int total = 0;
        for (auto& e : entries) total += e.points;
        auto lbl = CCLabelBMFont::create(fmt::format("{}", total).c_str(), "bigFont.fnt");
        lbl->setScale(.4f);
        lbl->setAnchorPoint({ 0.f, .5f });
        lbl->setPosition({ x, 0.f });
        pts->addChild(lbl);

        pts->setPosition(main->convertToNodeSpace({ rowRight + 14.f, rowY }));
        main->addChild(pts, 20);
    }

    (void)h;
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
            decorateCell(self, list);
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

    // игра обновляет сложность после загрузки уровня — применяем ещё раз
    void updateLabelValues() {
        LevelInfoLayer::updateLabelValues();
        if (!m_level) return;
        auto entries = GSLManager::get()->find(m_level->m_levelID.value());
        if (!entries || entries->empty()) return;
        Ref<LevelInfoLayer> self = this;
        auto list = *entries;
        Loader::get()->queueInMainThread([self, list] {
            if (auto diff = findDifficulty(self)) applyGSL(self, diff, list, true);
        });
    }
};

// Кнопка листа в окне поиска уровней
class $modify(GSLSearchLayer, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;
        if (!Mod::get()->getSettingValue<bool>("show-gsl-button")) return true;

        // круглая кнопка с логотипом GSL
        auto logo = CCSprite::create("gsl_btn.png"_spr);
        logo->setScale(34.f / logo->getContentWidth());
        auto spr = CircleButtonSprite::create(logo, CircleBaseColor::Green, CircleBaseSize::Medium);
        spr->setScale(.85f);
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

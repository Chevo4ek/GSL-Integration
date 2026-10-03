#include "GSLManager.hpp"
#include <algorithm>

static constexpr auto API = "https://globalshittylist.com/api/v1";
static constexpr int PAGE = 500;

GSLManager* GSLManager::get() {
    static GSLManager* inst = [] {
        auto m = new GSLManager();
        m->retain(); // живёт всё время работы игры
        return m;
    }();
    return inst;
}

void GSLManager::start() {
    if (m_started) return;
    m_started = true;

    loadCache();
    checkForChanges();

    auto interval = static_cast<float>(Mod::get()->getSettingValue<int64_t>("poll-interval"));
    CCScheduler::get()->scheduleSelector(
        schedule_selector(GSLManager::tick), this, interval, false
    );
}

void GSLManager::tick(float) {
    checkForChanges();
}

void GSLManager::forceRefresh() {
    if (m_loading) return;
    m_version.clear();
    checkForChanges();
}

// Версия базы = id последней записи changelog + количество уровней.
// Изменилось что-то одно — значит, лист обновился (в т.ч. удаление уровня).
void GSLManager::checkForChanges() {
    if (m_loading) return;

    m_checkListener.spawn(
        web::WebRequest().get(fmt::format("{}/changelog?limit=1", API)),
        [this](web::WebResponse res) {
            if (!res.ok()) { log::warn("GSL: changelog request failed ({})", res.code()); return; }

            auto json = res.json().unwrapOr(matjson::Value());
            std::string lastChange;
            if (auto arr = json["data"].asArray(); arr && !arr.unwrap().empty()) {
                lastChange = arr.unwrap()[0]["id"].asString().unwrapOr("");
            }

            m_statsListener.spawn(
                web::WebRequest().get(fmt::format("{}/stats", API)),
                [this, lastChange](web::WebResponse res) {
                    if (!res.ok()) { log::warn("GSL: stats request failed ({})", res.code()); return; }

                    auto json = res.json().unwrapOr(matjson::Value());
                    auto count = json["data"]["levels"].asInt().unwrapOr(-1);
                    auto version = lastChange + "|" + std::to_string(count);

                    if (version == m_version && !m_all.empty()) return; // ничего не поменялось

                    log::info("GSL: database changed ({} -> {}), reloading", m_version, version);
                    m_pendingVersion = version;
                    m_pending.clear();
                    m_loading = true;
                    fetchPage(0);
                }
            );
        }
    );
}

void GSLManager::fetchPage(int offset) {
    m_pageListener.spawn(
        web::WebRequest().get(
            fmt::format("{}/levels?limit={}&offset={}&sort=rank&order=asc", API, PAGE, offset)
        ),
        [this, offset](web::WebResponse res) {
            if (!res.ok()) {
                log::warn("GSL: levels request failed ({})", res.code());
                m_loading = false;
                return;
            }

            auto json = res.json().unwrapOr(matjson::Value());
            if (auto arr = json["data"].asArray()) {
                for (auto& v : arr.unwrap()) m_pending.push_back(v);
            }

            if (json["meta"]["hasMore"].asBool().unwrapOr(false)) {
                // следующую страницу запускаем после выхода из этого колбэка
                Loader::get()->queueInMainThread([this, offset] { fetchPage(offset + PAGE); });
                return;
            }

            // Всё скачано — заменяем лист целиком, удалённые уровни исчезнут
            matjson::Value all = matjson::Value::array();
            for (auto& v : m_pending) all.push(v);
            m_pending.clear();

            applyLevels(all);
            m_version = m_pendingVersion;
            Mod::get()->setSavedValue("cache-levels", all);
            Mod::get()->setSavedValue("cache-version", m_version);
            m_loading = false;

            log::info("GSL: loaded {} levels", m_all.size());
        }
    );
}

void GSLManager::applyLevels(matjson::Value const& arr) {
    m_all.clear();
    m_byId.clear();

    auto items = arr.asArray();
    if (!items) return;

    for (auto& v : items.unwrap()) {
        GSLEntry e;
        e.list    = v["list"].asString().unwrapOr("");
        e.name    = v["name"].asString().unwrapOr("?");
        e.author  = v["author"].asString().unwrapOr("");
        e.rank    = static_cast<int>(v["rank"].asInt().unwrapOr(0));
        e.levelId = static_cast<int>(v["levelId"].asInt().unwrapOr(0));
        e.points  = static_cast<int>(v["points"].asInt().unwrapOr(0));
        if (e.levelId <= 0 || e.list.empty()) continue;

        m_all.push_back(e);
        m_byId[e.levelId].push_back(e);
    }
}

void GSLManager::loadCache() {
    auto cached = Mod::get()->getSavedValue<matjson::Value>("cache-levels");
    if (cached.isArray()) {
        applyLevels(cached);
        m_version = Mod::get()->getSavedValue<std::string>("cache-version");
        log::info("GSL: {} levels loaded from cache", m_all.size());
    }
}

std::vector<GSLEntry> const* GSLManager::find(int levelId) const {
    auto it = m_byId.find(levelId);
    return it == m_byId.end() ? nullptr : &it->second;
}

std::vector<GSLEntry> GSLManager::getList(std::string const& list) const {
    std::vector<GSLEntry> out;
    for (auto& e : m_all) if (e.list == list) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.rank < b.rank; });
    return out;
}

std::string GSLManager::prettyListName(std::string const& list) {
    if (list == "classic") return "Classic";
    if (list == "challenge") return "Challenge";
    if (list == "spam") return "Spam";
    if (list == "future") return "Future";
    if (list == "future_challenge") return "Future Challenge";
    if (list == "future_spam") return "Future Spam";
    if (list == "impossible") return "Impossible";
    return list;
}

ccColor3B GSLManager::listColor(std::string const& list) {
    if (list == "classic") return { 255, 200, 60 };
    if (list == "challenge") return { 255, 90, 90 };
    if (list == "spam") return { 120, 220, 255 };
    if (list == "impossible") return { 200, 120, 255 };
    return { 180, 255, 140 }; // future*
}

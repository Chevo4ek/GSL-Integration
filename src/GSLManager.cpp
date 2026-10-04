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

            bool hadList = !m_all.empty();
            applyLevels(all);
            m_version = m_pendingVersion;
            if (hadList && Mod::get()->getSettingValue<bool>("notify-updates")) {
                Notification::create("Global Shitty List updated", NotificationIcon::Info)->show();
            }
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
        e.difficulty = v["difficulty"].asString().unwrapOr("");
        e.rank    = static_cast<int>(v["rank"].asInt().unwrapOr(0));
        e.levelId = static_cast<int>(v["levelId"].asInt().unwrapOr(0));
        e.points  = static_cast<int>(v["points"].asInt().unwrapOr(0));
        // rate: число или строка — показываем как есть, без лишних нулей
        if (v.contains("rate")) {
            auto& r = v["rate"];
            if (r.isString()) e.rate = r.asString().unwrapOr("");
            else if (r.isNumber()) {
                auto num = r.asDouble().unwrapOr(0.0);
                e.rate = fmt::format("{:.2f}", num);
                while (!e.rate.empty() && e.rate.back() == '0') e.rate.pop_back();
                if (!e.rate.empty() && e.rate.back() == '.') e.rate.pop_back();
            }
        }
        // категория / качество / «новый» — названия полей на сайте могут отличаться,
        // берём первое найденное
        auto str = [&](std::initializer_list<char const*> keys) {
            for (auto k : keys) {
                if (!v.contains(k)) continue;
                auto& f = v[k];
                if (f.isString()) return f.asString().unwrapOr("");
                if (f.isObject() && f.contains("name")) return f["name"].asString().unwrapOr("");
                if (f.isNumber()) return fmt::format("{}", f.asInt().unwrapOr(0));
            }
            return std::string();
        };
        e.category = str({ "category", "tier", "section", "group" });
        e.quality  = str({ "quality", "qualityTier", "quality_tier", "qualityLevel" });
        for (auto k : { "isNew", "is_new", "new" }) {
            if (v.contains(k) && v[k].isBool()) { e.isNew = v[k].asBool().unwrapOr(false); break; }
        }

        // victors: строки или объекты — берём ник из любого подходящего поля
        if (auto vs = v["victors"].asArray()) {
            for (auto& vic : vs.unwrap()) {
                std::string name;
                if (vic.isString()) name = vic.asString().unwrapOr("");
                else if (vic.isObject()) {
                    for (auto key : { "name", "username", "player", "user", "victor" }) {
                        if (!vic.contains(key)) continue;
                        auto& f = vic[key];
                        if (f.isString()) name = f.asString().unwrapOr("");
                        else if (f.isObject()) name = f["name"].asString().unwrapOr("");
                        if (!name.empty()) break;
                    }
                }
                if (!name.empty()) e.victors.push_back(name);
            }
        }
        if (e.levelId <= 0 || e.list.empty()) continue;
        // берём только списки, которые сейчас есть на сайте
        auto& ls = lists();
        if (std::find(ls.begin(), ls.end(), e.list) == ls.end()) continue;

        m_all.push_back(e);
        m_byId[e.levelId].push_back(e);
    }
}

void GSLManager::loadCache() {
    auto gd = Mod::get()->getSavedValue<matjson::Value>("gd-info");
    if (gd.isObject()) {
        for (auto& val : gd) {
            auto key = val.getKey().value_or("0");
            GDInfo info;
            info.length = static_cast<int>(val[0].asInt().unwrapOr(-1));
            info.stars = static_cast<int>(val[1].asInt().unwrapOr(0));
            m_gd[std::atoi(key.c_str())] = info;
        }
    }

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

std::vector<std::string> const& GSLManager::categoryOrder() {
    static std::vector<std::string> const order = { "Main", "Extended", "Advanced", "Unbounded" };
    return order;
}

std::string GSLManager::categoryOf(GSLEntry const& e) {
    if (!e.category.empty()) {
        // приводим к виду "Main", "Extended", ...
        std::string c = e.category;
        for (auto& ch : c) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (!c.empty()) c[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(c[0])));
        return c;
    }
    if (e.rank <= 75) return "Main";
    if (e.rank <= 150) return "Extended";
    if (e.rank <= 300) return "Advanced";
    return "Unbounded";
}

std::string GSLManager::prettyListName(std::string const& list) {
    if (list == "classic") return "Classic";
    if (list == "challenge") return "Challenge";
    if (list == "spam") return "Spam";
    if (list == "future") return "Future Classic";
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

std::vector<std::string> const& GSLManager::lists() {
    static std::vector<std::string> const ls = { "classic", "challenge", "spam", "future" };
    return ls;
}

std::string GSLManager::shortListName(std::string const& list) {
    if (list == "classic") return "CL";
    if (list == "challenge") return "CH";
    if (list == "spam") return "SP";
    if (list == "future") return "FC";
    return list;
}

// ---------- длина и звёзды с серверов GD ----------

GDInfo const* GSLManager::gdInfo(int levelId) const {
    auto it = m_gd.find(levelId);
    return it == m_gd.end() ? nullptr : &it->second;
}

void GSLManager::rememberLevel(GJGameLevel* level) {
    if (!level) return;
    GDInfo info;
    info.length = level->isPlatformer() ? 5 : static_cast<int>(level->m_levelLength);
    info.stars = level->m_stars.value();
    m_gd[level->m_levelID.value()] = info;
}

void GSLManager::saveGDInfo() {
    auto obj = matjson::Value::object();
    for (auto& [id, info] : m_gd) {
        if (info.length < 0) continue;
        auto arr = matjson::Value::array();
        arr.push(info.length);
        arr.push(info.stars);
        obj[std::to_string(id)] = arr;
    }
    Mod::get()->setSavedValue("gd-info", obj);
}

void GSLManager::requestGDInfo(std::vector<int> const& ids, std::function<void()> done) {
    for (int id : ids) {
        if (m_gd.contains(id)) continue;
        if (std::find(m_gdQueue.begin(), m_gdQueue.end(), id) == m_gdQueue.end()) m_gdQueue.push_back(id);
    }
    if (done) m_gdCallbacks.push_back(std::move(done));
    if (!m_gdLoading) fetchGDChunk();
}

// По 50 уровней за запрос, по очереди — чтобы не долбить сервер GD
void GSLManager::fetchGDChunk() {
    if (m_gdQueue.empty()) {
        m_gdLoading = false;
        saveGDInfo();
        auto callbacks = std::move(m_gdCallbacks);
        m_gdCallbacks.clear();
        for (auto& cb : callbacks) cb();
        return;
    }
    m_gdLoading = true;

    size_t n = std::min<size_t>(50, m_gdQueue.size());
    std::vector<int> chunk(m_gdQueue.begin(), m_gdQueue.begin() + n);
    m_gdQueue.erase(m_gdQueue.begin(), m_gdQueue.begin() + n);

    std::string ids;
    for (int id : chunk) {
        if (!ids.empty()) ids += ",";
        ids += std::to_string(id);
    }

    auto req = web::WebRequest();
    req.userAgent("");
    req.bodyString(fmt::format("gameVersion=22&binaryVersion=47&type=19&str={}&secret=Wmfd2893gb7", ids));
    req.header("Content-Type", "application/x-www-form-urlencoded");

    m_gdListener.spawn(
        req.post("https://www.boomlings.com/database/getGJLevels21.php"),
        [this, chunk](web::WebResponse res) {
            auto body = res.ok() ? res.string().unwrapOr("") : std::string();
            if (body.empty() || body[0] == '-') log::warn("GSL: GD level info request failed ({})", res.code());

            // "1:id:...:15:length:...:18:stars|..." # остальное
            auto levels = std::string(body.substr(0, body.find('#')));
            for (auto& lvl : utils::string::split(levels, "|")) {
                auto parts = utils::string::split(lvl, ":");
                int id = 0;
                GDInfo info;
                for (size_t i = 0; i + 1 < parts.size(); i += 2) {
                    auto& k = parts[i];
                    int v = std::atoi(parts[i + 1].c_str());
                    if (k == "1") id = v;
                    else if (k == "15") info.length = v;
                    else if (k == "18") info.stars = v;
                }
                if (id > 0) m_gd[id] = info;
            }
            // уровни, которых сервер не вернул (или запрос не прошёл), помечаем как неизвестные,
            // чтобы не спрашивать по кругу. На диск неизвестные не сохраняются.
            for (int id : chunk) if (!m_gd.contains(id)) m_gd[id] = GDInfo{};

            Loader::get()->queueInMainThread([this] { fetchGDChunk(); });
        }
    );
}

#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <unordered_map>
#include <vector>
#include <string>

using namespace geode::prelude;

struct GSLEntry {
    std::string list;     // classic, challenge, spam, future, ...
    std::string name;
    std::string author;
    std::string difficulty; // easy_demon ... extreme_demon
    int rank = 0;
    int levelId = 0;
    int points = 0;
    std::vector<std::string> victors; // ники прошедших
};

// Данные уровня с серверов GD: длина и звёзды (для фильтров)
struct GDInfo {
    int length = -1; // 0 Tiny, 1 Short, 2 Medium, 3 Long, 4 XL, 5 Platformer
    int stars = 0;
};

// Хранит лист, синхронизирует его с сайтом и сохраняет кэш на диск.
class GSLManager : public CCObject {
public:
    static GSLManager* get();

    void start();
    void forceRefresh();

    // Все позиции уровня (уровень может быть в нескольких списках)
    std::vector<GSLEntry> const* find(int levelId) const;
    // Весь список по имени, отсортированный по месту
    std::vector<GSLEntry> getList(std::string const& list) const;

    bool isLoading() const { return m_loading; }
    size_t total() const { return m_all.size(); }

    // Длина/звёзды уровней. requestGDInfo докачивает недостающие и потом зовёт done.
    GDInfo const* gdInfo(int levelId) const;
    void rememberLevel(GJGameLevel* level);
    void requestGDInfo(std::vector<int> const& ids, std::function<void()> done);
    bool isLoadingGDInfo() const { return m_gdLoading; }

    static std::string prettyListName(std::string const& list);
    static std::string shortListName(std::string const& list);
    static ccColor3B listColor(std::string const& list);
    static std::vector<std::string> const& lists();

private:
    void tick(float);
    void checkForChanges();
    void fetchPage(int offset);
    void applyLevels(matjson::Value const& arr);
    void loadCache();
    void fetchGDChunk();
    void saveGDInfo();

    std::vector<GSLEntry> m_all;
    std::unordered_map<int, std::vector<GSLEntry>> m_byId;

    std::vector<matjson::Value> m_pending;
    std::string m_pendingVersion;
    std::string m_version;
    bool m_loading = false;
    bool m_started = false;

    async::TaskHolder<web::WebResponse> m_checkListener;
    async::TaskHolder<web::WebResponse> m_statsListener;
    async::TaskHolder<web::WebResponse> m_pageListener;

    std::unordered_map<int, GDInfo> m_gd;
    std::vector<int> m_gdQueue;
    std::vector<std::function<void()>> m_gdCallbacks;
    bool m_gdLoading = false;
    async::TaskHolder<web::WebResponse> m_gdListener;
};

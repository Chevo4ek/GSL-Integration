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
    int rank = 0;
    int levelId = 0;
    int points = 0;
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

    static std::string prettyListName(std::string const& list);
    static ccColor3B listColor(std::string const& list);

private:
    void tick(float);
    void checkForChanges();
    void fetchPage(int offset);
    void applyLevels(matjson::Value const& arr);
    void loadCache();

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
};

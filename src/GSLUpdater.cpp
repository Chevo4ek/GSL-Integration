#include "GSLUpdater.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/hash.hpp>

using namespace geode::prelude;

// Откуда брать обновления: публичный репозиторий только с готовыми файлами
// (код мода остаётся в приватном). В нём лежат:
//   latest.json — {"version": "v0.0.2", "sha256": "..."}
//   chevo4ek.gsl-integration.geode — сам мод
static constexpr auto UPDATE_BASE = "https://raw.githubusercontent.com/Chevo4ek/GSL-Integration-Releases/main/";

static async::TaskHolder<web::WebResponse> s_check, s_file;
static bool s_busy = false;

static web::WebRequest request() {
    auto req = web::WebRequest();
    req.userAgent("GSL-Integration-Updater");
    return req;
}

static void fail(std::string const& why) {
    s_busy = false;
    log::warn("GSL update: {}", why);
    FLAlertLayer::create("Update failed", why, "OK")->show();
}

static void install(std::string version, std::string fileUrl, std::string expected) {
    s_busy = true;
    auto note = Notification::create("Downloading update...", NotificationIcon::Loading, 0.f);
    note->show();
    Ref<Notification> noteRef = note;
    for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    s_file.spawn(request().get(fileUrl), [=](web::WebResponse res) {
        noteRef->hide();
        if (!res.ok()) return fail("Couldn't download the update.");
        auto const& data = res.data();
        auto actual = sha256(std::span<uint8_t const>(data.data(), data.size())).toString();
        if (actual != expected) return fail("The downloaded file doesn't match its checksum, nothing was changed.");

        auto path = Mod::get()->getPackagePath();
        if (auto w = file::writeBinarySafe(path, data); !w) {
            return fail(fmt::format("Couldn't save the update: {}", w.unwrapErr()));
        }
        s_busy = false;
        createQuickPopup(
            "Updated",
            fmt::format("<cg>Global Shitty List Integration {}</c> is installed.\nRestart the game to use it.", version),
            "Later", "Restart",
            [](FLAlertLayer*, bool restart) { if (restart) game::restart(true); }
        );
    });
}

void gslCheckForUpdate(bool manual) {
    if (s_busy) return;
    // ?t=... — чтобы GitHub не отдавал старую закэшированную версию
    auto url = fmt::format("{}latest.json?t={}", UPDATE_BASE, std::time(nullptr));
    s_check.spawn(request().get(url), [manual](web::WebResponse res) {
        if (!res.ok()) {
            log::warn("GSL update check failed ({})", res.code());
            if (manual) FLAlertLayer::create("Update", "Couldn't check for updates.", "OK")->show();
            return;
        }
        auto json = res.json().unwrapOr(matjson::Value());
        auto latest = VersionInfo::parse(json["version"].asString().unwrapOr(""));
        auto sum = json["sha256"].asString().unwrapOr("");
        if (!latest || sum.size() != 64) return;
        if (!(latest.unwrap() > Mod::get()->getVersion())) {
            if (manual) FLAlertLayer::create("Update", "You have the latest version.", "OK")->show();
            return;
        }
        auto fileUrl = fmt::format("{}{}.geode?t={}", UPDATE_BASE, Mod::get()->getID(), std::time(nullptr));
        auto version = latest.unwrap().toVString();
        createQuickPopup(
            "Update available",
            fmt::format("Version <cy>{}</c> of Global Shitty List Integration is out.\nYou have <cr>{}</c>. Update now?",
                version, Mod::get()->getVersion().toVString()),
            "Later", "Update",
            [version, fileUrl, sum](FLAlertLayer*, bool update) {
                if (update) install(version, fileUrl, sum);
            }
        );
    });
}
